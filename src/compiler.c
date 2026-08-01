#include "dcc.h"

static FILE *inputFile  = NULL;
static int   lineNumber = 1;
static char *fileName   = NULL;

static int lineRewind = 1;
static int offsetRewind = 0;

static bool  error      = false;

static typedefs_t     typedefs        = {0};
static variableList_t globalVariables = {0};
static functionList_t functions       = {0};


char *g_keywords[] =
{
    "int", "char", "do", "while", "for", "if", "else", "const",
    "static", "return", "continue", "break", "typedef", "struct",
    "void", "NULL", "unsigned",
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

void drainToEndOfBlock(bool alreadyInBlock)
{
    char *token      = NULL;
    int   braceDepth = 0;

    if (alreadyInBlock)
    {
        braceDepth++;
    }

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

// The opening "{" has already been parsed
static void processStackFrame(stackFrame_t *stackFrame)
{
    char *token = NULL;
    bool  rc    = true;

    stackFrame_t *tmpStackFrame = NULL;
    variable_t   *tmpVar        = NULL;
    type_t       *tmpType       = NULL;

    if (NULL == stackFrame)
    {
        INTERNAL_ERROR;
        return;
    }

    while (true)
    {
        saveForRewindTokenParse();

        if (NULL != token)
        {
            free(token);
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            printError("Expected \"}\"");
            return;
        }

        if (0 == strcmp("}", token))
        {
            free(token);
            token = NULL;

            if (rc)
            {
                return;
            }
            break;
        }

        if (0 == strcmp("if", token))
        {
            //TODO
            printError("if statements not implemented");
            break;
        }
        if (0 == strcmp("else", token))
        {
            //TODO
            printError("else statements not implemented");
            break;
        }
        if (0 == strcmp("for", token))
        {
            //TODO
            printError("for statements not implemented");
            break;
        }
        if (0 == strcmp("do", token))
        {
            //TODO
            printError("do statements not implemented");
            break;
        }
        if (0 == strcmp("while", token))
        {
            //TODO
            printError("while statements not implemented");
            break;
        }

        if (0 == strcmp("{", token))
        {
            free(token);
            tmpStackFrame = calloc(1, sizeof(*stackFrame));
            tmpStackFrame->prevAccessableStackFrame = stackFrame;
            processStackFrame(tmpStackFrame);
            continue;
        }

        if (0 == strcmp("unsigned", token) ||
            0 == strcmp("short",    token) ||
            NULL != (tmpType = findType(token)))
        {
            // Declaring/defining a variable
            free(token);
            token = NULL;


            rewindTokenParse();
            if (NULL != tmpType)
            {
                free(tmpType);
                tmpType = NULL;
            }

            if (false == getTypeAndIdent(&tmpType, &token))
            {
                drainToNextSemicolon();
                continue;
            }

            if (NULL != (tmpVar = findVariable(&(stackFrame->variables), token)))
            {
                tmpVar = NULL;
                printError("Redefinition of variable");
                free(tmpType);
                tmpType = NULL;
                drainToNextSemicolon();
                continue;
            }

            tmpVar = calloc(1, sizeof(*tmpVar));

            tmpVar->identifier = token;
            token              = NULL;

            memcpy(&(tmpVar->type), tmpType, sizeof(*tmpType));
            free(tmpType);
            tmpType = NULL;

            if (NULL == stackFrame->variables.first)
            {
                stackFrame->variables.first = tmpVar;
                stackFrame->variables.last  = tmpVar;
            }
            else
            {
                stackFrame->variables.last->next = tmpVar;
                stackFrame->variables.last       = tmpVar;
            }

            token = getNextTokenCheckingForLineChange();

            if (NULL == token)
            {
                printError("expected \";\" or \"=\"");
                drainToNextSemicolon();
                rc = false;
                continue;
            }

            if (0 == strcmp(";", token))
            {
                tmpVar = NULL;
                continue;
            }

            if (0 != strcmp("=", token))
            {
                printError("expected \";\" or \"=\"");
                drainToNextSemicolon();
                rc = false;
                continue;
            }

            printError("simultaneous declaration and assignment not supported");
            drainToNextSemicolon();
            rc = false;
            continue;
        }

        printError("No clue what you're doing, but I haven't implemented it yet");
        drainToNextSemicolon();
    }

    // Error path
    drainToEndOfBlock(true);
    freeVoidListContents(&(stackFrame->codeBlock));
    freeVariableList(&(stackFrame->variables));
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
        while (true)
        {
            if (NULL != token)
            {
                free(token);
            }
            token = getNextTokenCheckingForLineChange();
            if (NULL == token)
            {
                return;
            }
            if (0 == strcmp(";", token))
            {
                free(token);
                return;
            }
            if (0 == strcmp("{", token))
            {
                free(token);
                drainToEndOfBlock(true);
                return;
            }
        }
    }

    if (NULL != token)
    {
        free(token);
    }

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        (0 != strcmp(";", token) &&
         0 != strcmp("{", token)))
    {
        printError("expected \";\" or \"{\"");
        return;
    }

    if (0 == strcmp("{", token))
    {
        processStackFrame(&(newFunction.definition));
        newFunction.isDefined = true;
    }

    free(token);


    memcpy(&newFunction.returnType, returnType, sizeof(*returnType));
    newFunction.identifier = strcpy(calloc(strlen(identifier) + 1, sizeof(char)), identifier);

    if (NULL == functions.first)
    {
        functions.first = memcpy(calloc(1, sizeof(newFunction)), &newFunction, sizeof(newFunction));
        functions.last  = functions.first;
    }
    else
    {
        functions.last->next = memcpy(calloc(1, sizeof(newFunction)), &newFunction, sizeof(newFunction));
        functions.last       = functions.last->next;
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

    return !error;
}

// DEBUG

#define PRINT_DEBUG_BANNER printf("\n========================================"\
                                    "========================================\n\n")

static void DEBUG_printType(type_t *t)
{
    if (NULL == t)
    {
        INTERNAL_ERROR;
        return;
    }

    if (t->isVoid)
    {
        printf("void");
    }
    else if (t->isRaw)
    {
        switch (t->type.rawType)
        {
            case int_e:
            {
                printf("int");
                break;
            }
            case uint_e:
            {
                printf("uint");
                break;
            }
            case hint_e:
            {
                printf("hint");
                break;
            }
            case huint_e:
            {
                printf("huint");
                break;
            }
            case char_e:
            {
                printf("char");
                break;
            }
            case uchar_e:
            {
                printf("uchar");
                break;
            }
            default:
            {
                INTERNAL_ERROR;
                break;
            }
        }
    }
    else if (NULL != t->type.typeDefinition)
    {
        printf("%s", t->type.typeDefinition->identifier);
    }
    else
    {
        INTERNAL_ERROR;
        return;
    }

    for (int i = 0; i < t->pointerDepth; i++)
    {
        printf("*");
    }
}

static void DEBUG_printPadding(int padding)
{
    for (int i = 0; i < padding; i++)
    {
        printf(" ");
    }
}

static void DEBUG_printStackFrame(int padding, stackFrame_t *sf)
{
    if (padding < 0 ||
        NULL == sf)
    {
        INTERNAL_ERROR;
        return;
    }

    DEBUG_printPadding(padding);
    printf("variables:\n");
    if (NULL == sf->variables.first)
    {
        DEBUG_printPadding(padding + 2);
        printf("NONE\n");
    }
    else
    {
        for (variable_t *v = sf->variables.first; NULL != v; v = v->next)
        {
            DEBUG_printPadding(padding + 2);
            printf ("%s is of type ", v->identifier);
            DEBUG_printType(&v->type);
            printf("\n");
        }
    }
}

static void printDebug()
{
    PRINT_DEBUG_BANNER;
    printf("Typedefs:\n");
    if (NULL == typedefs.first)
    {
        printf("  NONE\n");
    }
    else
    {
        for (typedef_t *t = typedefs.first; NULL != t; t = t->next)
        {
            printf("  %s is ", t->identifier);
            switch (t->typedefType)
            {
                case typeExtension_e:
                {
                    printf("of type ");
                    DEBUG_printType(&t->content.typeExtension);
                    printf("\n");
                    break;
                }
                case typeStructDefinition_e:
                {
                    printf("a structure <not yet represented>\n");
                    break;
                }
                case typeEnumDefinition_e:
                {
                    printf("an enum <not yet represented>\n");
                    break;
                }
                default:
                {
                    printf("broken\n");
                    break;
                }
            }
        }
    }

    PRINT_DEBUG_BANNER;

    printf("Global variables:\n");
    if (NULL == globalVariables.first)
    {
        printf("  NONE\n");
    }
    else
    {
        for (variable_t *v = globalVariables.first; NULL != v; v = v->next)
        {
            printf ("  %s is of type ", v->identifier);
            DEBUG_printType(&v->type);
            printf("\n");
        }
    }

    PRINT_DEBUG_BANNER;

    printf("Functions:\n");
    if (NULL == functions.first)
    {
        printf(" NONE\n");
    }
    else
    {
        for (function_t *f = functions.first; NULL != f; f = f->next)
        {
            printf("  %s:\n", f->identifier);

            printf("    returns ");
            DEBUG_printType(&f->returnType);
            printf("\n");

            printf("    parameters:\n");
            if (NULL == f->parameterNames.first ||
                NULL == f->parameterTypes.first)
            {
                printf("      NONE\n");
            }
            else
            {
                strll_t *i = f->parameterNames.first;
                type_t  *t = f->parameterTypes.first;
                
                while (i != NULL && t != NULL)
                {
                    printf("      ");
                    DEBUG_printType(t);
                    printf(" (called %s)\n", i->str);

                    i = i->next;
                    t = t->next;
                }
            }

            printf("    definition:\n");
            if (false == f->isDefined)
            {
                printf("      NONE\n");
            }
            else
            {
                DEBUG_printStackFrame(6, &f->definition);
            }

            if (NULL != f->next)
            {
                printf("\n");
            }
        }
    }

    PRINT_DEBUG_BANNER;
}
// DEBUG

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

    // DEBUG
    printDebug();
    // DEBUG

    return true;
}
