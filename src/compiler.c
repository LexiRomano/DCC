#include "dcc.h"

static FILE *inputFile  = NULL;
static int   lineNumber = 1;
static char *fileName   = NULL;

static int lineRewind = 1;
static int offsetRewind = 0;

static bool  error      = false;

static typedefs_t     typedefs        = {0};
static variableList_t globalVariables = {0};
//static functionList_t functions       = {0};


char *g_keywords[] =
{
    "int", "char", "do", "while", "for", "if", "else", "const",
    "static", "return", "continue", "break", "typedef", "struct",
    "void", "NULL", "unsigned",
    NULL
};

// DEBUG
static void DEBUG_printType(type_t *t)
{
    if (NULL == t)
    {
        INTERNAL_ERROR;
        return;
    }

    if (t->isRaw)
    {
        printf("TYPE: raw (%d)\n", t->type.rawType);
    }
    else
    {
        printf("TYPE: typedef (%s)\n", t->type.typeDefinition->identifier);
    }
}
// DEBUG

static char *getNextTokenCheckingForLineChange()
{
    char *nextToken = getNextTokenFromFile(inputFile, &lineNumber);

    if (NULL == nextToken)
    {
        return NULL;
    }

    /*
    if (0 == strcmp("#", nextToken))
    {
        // TODO
    }
    */

    return nextToken;
}

static void saveForRewindTokenParse()
{
    lineRewind   = lineNumber;
    offsetRewind = ftell(inputFile);
}

static void rewindTokenParse()
{
    lineNumber = lineRewind;
    fseek(inputFile, offsetRewind, SEEK_SET);
}

void drainToNextSemicolon()
{
    char *token        = NULL;
    int   bracketDepth = 0;
    int   braceDepth   = 0;
    int   parenDepth   = 0;

    while (true)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            return;
        }

        if (0 == bracketDepth &&
            0 == braceDepth   &&
            0 == parenDepth   &&
            0 == strcmp(";", token))
        {
            free(token);
            return;
        }

        if (0 == strcmp("[", token))
        {
            bracketDepth++;
        }
        else if (0 == strcmp("]", token))
        {
            bracketDepth--;
        }
        else if (0 == strcmp("{", token))
        {
            braceDepth++;
        }
        else if (0 == strcmp("}", token))
        {
            braceDepth--;
        }
        else if (0 == strcmp("(", token))
        {
            parenDepth++;
        }
        else if (0 == strcmp(")", token))
        {
            parenDepth--;
        }
    }
}

void drainToEndOfBlock()
{
    char *token      = NULL;
    int   braceDepth = 0;

    while (true)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            return;
        }

        if (0 == strcmp("{", token))
        {
            braceDepth++;
        }
        else if (0 == strcmp("}", token))
        {
            braceDepth--;
            if (0 == braceDepth)
            {
                return;
            }
        }
    }
}

static void printErrorDuplicateKeyword(char *keyword)
{
    char *line = getCurrentLine(inputFile);

    ERROR_ARGS(fileName, lineNumber, line, "duplicate keyword \"%s\"", keyword);

    free(line);

    error = true;
}

static void printError(char *message)
{
    char *line = getCurrentLine(inputFile);

    ERROR_ARGS(fileName, lineNumber, line, "%s", message);

    free(line);

    error = true;
}

// Caller to free result
static type_t *findType(char *str)
{
    type_t *out = NULL;

    if (0 == strcmp(str, "int"))
    {
        out = calloc(1, sizeof(*out));

        out->isRaw = true;
        out->type.rawType = int_e;

        return out;
    }

    if (0 == strcmp(str, "char"))
    {
        out = calloc(1, sizeof(*out));

        out->isRaw = true;
        out->type.rawType = char_e;

        return out;
    }

    if (0 == strcmp(str, "void"))
    {
        out = calloc(1, sizeof(*out));

        out->isVoid = true;
        
        return out;
    }

    for (typedef_t *t = typedefs.first; NULL != t; t = t->next)
    {
        if (0 == strcmp(str, t->identifier))
        {
            out = calloc(1, sizeof(*out));
            out->isRaw = false;
            out->type.typeDefinition = t;

            return out;
        }
    }

    return NULL;
}

static bool createVariable(type_t *varType,
                           char *identifier,
                           variableList_t *targetVarList,
                           bool parseValue)
{
    variable_t *newVar = NULL;

    if (NULL == varType    ||
        NULL == identifier ||
        NULL == targetVarList)
    {
        return false;
    }

    if (parseValue)
    {
        //TODO
    }

    // DEBUG
    printf("New variable \"%s\" is ", identifier);
    DEBUG_printType(varType);
    // DEBUG

    newVar = calloc(1, sizeof(*newVar));

    newVar->identifier = strcpy(calloc(strlen(identifier) + 1, sizeof(char)), identifier);
    memcpy(&(newVar->type), varType, sizeof(*varType));

    if (NULL == targetVarList->first)
    {
        targetVarList->first = newVar;
        targetVarList->last  = newVar;
        return true;
    }

    targetVarList->last->next = newVar;
    targetVarList->last       = newVar;

    return true;
}

static bool applyVarDescriptors(bool isShort, bool isUnsigned, type_t *type)
{
    if (NULL == type)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (isShort &&
        isUnsigned)
    {
        if (false == type->isRaw ||
            int_e != type->type.rawType)
        {
            printError("cannot apply short and unsigned to this type");
            return false;
        }

        type->type.rawType = huint_e;
        return true;
    }

    if (isShort)
    {
        if (false == type->isRaw ||
            int_e != type->type.rawType)
        {
            printError("cannot apply short to this type");
            return false;
        }

        type->type.rawType = hint_e;

        return true;
    }

    if (isUnsigned)
    {
        if (false == type->isRaw ||
            (int_e  != type->type.rawType &&
             char_e != type->type.rawType))
        {
            printError("cannot apply unsigned to this type");
            return false;
        }

        if (int_e == type->type.rawType)
        {
            type->type.rawType = uint_e;
            return true;
        }

        type->type.rawType = uchar_e;
    }

    return true;
}

static bool getTypeAndIdent(type_t **typeOut,
                            char   **identOut)
{
    char *token = NULL;

    bool isUnsigned = false;
    bool isShort    = false;

    type_t *tmpType   = NULL;
    type_t *foundType = NULL;

    bool rc = true;

    if (NULL ==  typeOut  ||
        NULL != *typeOut  ||
        NULL ==  identOut ||
        NULL != *identOut)
    {
        INTERNAL_ERROR;
    }

    while (true)
    {
        if (NULL != token)
        {
            free(token);
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            printError("incomplete statement");
            return false;
        }

        if (0 == strcmp("unsigned", token))
        {
            if (isUnsigned)
            {
                printErrorDuplicateKeyword("unsigned");
                rc = false;
                continue;
            }
            isUnsigned = true;
            continue;
        }
        if (0 == strcmp("short", token))
        {
            if (isShort)
            {
                printErrorDuplicateKeyword("short");
                rc = false;
                continue;
            }
            isShort = true;
            continue;
        }
        if (NULL != (tmpType = findType(token)))
        {
            // Found the base variable type
            if (NULL != foundType)
            {
                printError("multiple types specified");
                free(tmpType);
                rc = false;
                continue;
            }

            foundType = tmpType;
            tmpType   = NULL;
            
            rc = rc && applyVarDescriptors(isShort, isUnsigned, foundType);

            continue;
        }

        if (isIdentifier(token))
        {
            if (NULL == foundType)
            {
                printError("expected type before identifier");
                return false;
            }
            if (rc)
            {
                *identOut = token;
                *typeOut  = foundType;

                return true;
            }

            free(token);

            if (NULL != foundType)
            {
                free(foundType);
            }
            
            return false;
        }

        if (isKeyword(token))
        {
            printError("unexpected keyword");
            return false;
        }

        printError("idk what you did, but this is wrong");
    }
}

static void doTypedef()
{
    char      *token      = NULL;
    type_t    *foundType  = NULL;
    char      *foundIdent = NULL;
    typedef_t *newTypedef = NULL;

    saveForRewindTokenParse();

    token = getNextTokenCheckingForLineChange();

    if (NULL == token)
    {
        printError("incomplete typedef");
        return;
    }

    if (0 == strcmp("struct", token))
    {
        // is a struct
        printError("structs not implemented");
        free(token);
        return;
    }
    if (0 == strcmp("enum", token))
    {
        // is an enum
        printError("enums not implemented");
        free(token);
        drainToNextSemicolon();
        return;
    }

    free(token);
    token = NULL;

    rewindTokenParse();

    if (false == getTypeAndIdent(&foundType, &foundIdent))
    {
        drainToNextSemicolon();
        return;
    }

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        0    != strcmp(";", token))
    {
        printError("Expected \";\"");
        if (NULL != token)
        {
            free(token);
        }
        return;
    }

    printf("Typedef \"%s\" complete as ", foundIdent);
    if (foundType->isRaw)
    {
        printf("raw (%d)\n", foundType->type.rawType);
    }
    else
    {
        printf("alias of \"%s\"\n", foundType->type.typeDefinition->identifier);
    }
    // DEBUG

    newTypedef = calloc(1, sizeof(*newTypedef));

    newTypedef->identifier = foundIdent;
    newTypedef->typedefType = typeExtension_e;
    memcpy(&(newTypedef->content.typeExtension), foundType, sizeof(*foundType));

    if (NULL == typedefs.first)
    {
        typedefs.first = newTypedef;
        typedefs.last  = newTypedef;
        return;
    }

    typedefs.last->next = newTypedef;
    typedefs.last       = newTypedef;
}

// The opening "(" has already been parsed
static void processFunction(type_t *returnType,
                            char   *identifier)
{
    function_t newFunction   = {0};
    char      *token         = NULL;
    bool       findArguments = false;
    type_t    *foundType     = NULL;
    char      *foundIdent    = NULL;
    strll_t   *tmpStrll      = NULL;

    if (NULL == returnType ||
        NULL == identifier)
    {
        INTERNAL_ERROR;
        return;
    }

    // DEBUG
    printf("Processing function \"%s\"\n", identifier);
    // DEBUG

    saveForRewindTokenParse();

    token = getNextTokenCheckingForLineChange();

    if (NULL == token)
    {
        printError("incomplete function declaration");
        return;
    }

    findArguments = true;
    if (0 == strcmp(")", token))
    {
        findArguments = false;
    }
    else if (0 == strcmp(",", token))
    {
        printError("expected a type specifier");
        return;
    }
    else
    {
        rewindTokenParse();
    }

    // Getting the arguments
    while (findArguments)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        if (false == getTypeAndIdent(&foundType, &foundIdent))
        {
            freeFunctionContents(&newFunction);
            return;
        }

        // DEBUG
        printf("  New parameter: \"%s\" of ", foundIdent);
        DEBUG_printType(foundType);
        // DEBUG

        tmpStrll = addStringLinkedList(&(newFunction.parameterNames));

        if (NULL == tmpStrll)
        {
            INTERNAL_ERROR;
            freeFunctionContents(&newFunction);
            return;
        }

        tmpStrll->str = foundIdent;
        foundIdent    = NULL;

        if (NULL == newFunction.parameterTypes.first)
        {
            newFunction.parameterTypes.first = foundType;
            newFunction.parameterTypes.last  = foundType;
        }
        else
        {
            newFunction.parameterTypes.last->next = foundType;
            newFunction.parameterTypes.last       = foundType;
        }

        foundType = NULL;
        

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            printError("incomplete function declaration");
            freeFunctionContents(&newFunction);
            return;
        }

        if (0 == strcmp(")", token))
        {
            break;
        }
        if (0 == strcmp(",", token))
        {
            continue;
        }

        printError("expected \",\" or \")\"");
        freeFunctionContents(&newFunction);
        return;
    }

    if (NULL != token)
    {
        free(token);
    }

    saveForRewindTokenParse();

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        (0 != strcmp(";", token) &&
         0 != strcmp("{", token)))
    {
        printError("expected \";\" or \"{\"");
        return;
    }

    free(token);

    rewindTokenParse();

    drainToEndOfBlock();

    //TODO
}

static bool intake()
{
    char *token = NULL;

    bool  isUnsigned = false;
    bool  isShort    = false;
    char *ident      = NULL;
    //int   pointerDepth = 0;

    bool  noMoreTokens = false;
    type_t *foundType = NULL;
    type_t *tmpType   = NULL;

    while (!noMoreTokens)
    {
        while (true)
        {
            if (NULL != token)
            {
                free(token);
            }
            token = getNextTokenCheckingForLineChange();

            if (NULL == token)
            {
                noMoreTokens = true;
                break;
            }

            if (NULL != ident)
            {
                if (0 == strcmp("=", token))
                {
                    // variable declaration + assignment
                    break;
                }
                if (0 == strcmp(";", token))
                {
                    // variable declaration

                    createVariable(foundType, ident, &globalVariables, false);

                    break;
                }
                if (0 == strcmp("(", token))
                {
                    // function declaration/definition
                    processFunction(foundType, ident);
                    break;
                }

                printError("expected \"=\", \";\", or \"(\"");
                drainToNextSemicolon();

                break;
            }

            if (0 == strcmp("typedef", token))
            {
                doTypedef();
                break;
            }
            if (0 == strcmp("unsigned", token))
            {
                if (isUnsigned)
                {
                    printErrorDuplicateKeyword("unsigned");
                    continue;
                }
                isUnsigned = true;
                continue;
            }
            if (0 == strcmp("short", token))
            {
                if (isShort)
                {
                    printErrorDuplicateKeyword("short");
                    continue;
                }
                isShort = true;
                continue;
            }
            if (NULL != (tmpType = findType(token)))
            {
                // Found the base variable type
                if (NULL != foundType)
                {
                    printError("multiple types specified");
                    free(tmpType);
                    continue;
                }

                foundType = tmpType;
                tmpType   = NULL;
                
                applyVarDescriptors(isShort, isUnsigned, foundType);

                continue;
            }

            if (isIdentifier(token))
            {
                if (NULL == foundType)
                {
                    printError("expected type before identifier");
                    drainToNextSemicolon();
                    break;
                }
                ident = token;
                token = NULL;
                continue;
            }

            if (isKeyword(token))
            {
                printError("unexpected keyword");
                drainToNextSemicolon();
                continue;
            }

            printError("idk what you did, but this is wrong");
            drainToNextSemicolon();
        }

        isUnsigned = false;
        isShort    = false;

        if (NULL != ident)
        {
            free(ident);
            ident = NULL;
        }

        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        if (NULL != tmpType)
        {
            free(tmpType);
            tmpType = NULL;
        }

        if (NULL != foundType)
        {
            free(foundType);
            foundType = NULL;
        }
    }

    return error;
}

bool compile(char *inputFileName, char *outputFileName)
{
    if (NULL == inputFileName ||
        NULL == outputFileName)
    {
        INTERNAL_ERROR;
        return false;
    }

    inputFile = fopen(inputFileName, "r");

    if (NULL == inputFile)
    {
        printf("Could not open %s\n", inputFileName);
        return false;
    }

    fileName = strcpy(calloc(strlen(inputFileName) + 1, sizeof(char)),
                      inputFileName);

    if (false == intake())
    {
        fclose(inputFile);
        free(fileName);
        return false;
    }

    fclose(inputFile);
    free(fileName);

    return true;
}
