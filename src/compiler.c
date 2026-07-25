#include "dcc.h"

static FILE *inputFile  = NULL;
static int   lineNumber = 1;
static char *fileName   = NULL;

static bool  error      = false;

static typedefs_t     typedefs        = {0};
static variableList_t globalVariables = {0};


char *g_keywords[] =
{
    "int", "char", "do", "while", "for", "if", "else", "const",
    "static", "return", "continue", "break", "typedef", "struct",
    "void", "NULL",
    NULL
};


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

    printf("New variable \"%s\" is ", identifier);
    if (varType->isRaw)
    {
        printf("raw (%d)\n", varType->type.rawType);
    }
    else
    {
        printf("a typedef (%s)\n", varType->type.typeDefinition->identifier);
    }

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

static void applyVarDescriptors(bool isShort, bool isUnsigned, type_t *type)
{
    if (NULL == type)
    {
        INTERNAL_ERROR;
    }

    if (isShort &&
        isUnsigned)
    {
        if (false == type->isRaw ||
            int_e != type->type.rawType)
        {
            printError("cannot apply short and unsigned to this type");
            return;
        }

        type->type.rawType = huint_e;
        return;
    }

    if (isShort)
    {
        if (false == type->isRaw ||
            int_e != type->type.rawType)
        {
            printError("cannot apply short to this type");
            return;
        }

        type->type.rawType = hint_e;
    }

    if (isUnsigned)
    {
        if (false == type->isRaw ||
            (int_e  != type->type.rawType &&
             char_e != type->type.rawType))
        {
            printError("cannot apply unsigned to this type");
            return;
        }

        if (int_e == type->type.rawType)
        {
            type->type.rawType = uint_e;
            return;
        }

        type->type.rawType = uchar_e;
    }
}

static void doTypedef()
{
    char *token       = NULL;
    char *ident       = NULL;
    bool  loopStarted = false;
    bool  isShort     = false;
    bool  isUnsigned  = false;

    type_t    *tmpType    = NULL;
    type_t    *foundType  = NULL;
    typedef_t *newTypedef = NULL;

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

    while (true)
    {
        if (loopStarted)
        {
            if (NULL != token)
            {
                free(token);
            }

            token = getNextTokenCheckingForLineChange();

            if (NULL == token)
            {
                printError("incomplete typedef");
            }
        }
        loopStarted = true;

        if (NULL != ident)
        {
            if (0 == strcmp(";", token))
            {
                free(token);
                token = NULL;

                printf("Typedef \"%s\" complete as ", ident);
                if (foundType->isRaw)
                {
                    printf("raw (%d)\n", foundType->type.rawType);
                }
                else
                {
                    printf("alias of \"%s\"\n", foundType->type.typeDefinition->identifier);
                }

                newTypedef = calloc(1, sizeof(*newTypedef));

                newTypedef->identifier = ident;
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
                return;
            }

            printError("expected \";\"");
            free(ident);
            free(token);
            drainToNextSemicolon();
            return;
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
                drainToNextSemicolon();
                return;
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
            return;
        }

        printError("idk what you did, but this is wrong");
    }
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
                    if (NULL == ident)
                    {
                        printError("expected identifier");
                        break;
                    }
                    break;
                }
                if (0 == strcmp(";", token))
                {
                    // variable declaration
                    if (NULL == ident)
                    {
                        printError("expected identifier");
                        break;
                    }

                    createVariable(foundType, ident, &globalVariables, false);

                    break;
                }
                if (0 == strcmp("(", token))
                {
                    // function declaration/definition
                    if (NULL == ident)
                    {
                        printError("expected identifier");
                        break;
                    }
                    break;
                }

                if (NULL != ident)
                {
                    printError("expected \"=\", \";\", or \"(\"");

                    break;
                }
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
