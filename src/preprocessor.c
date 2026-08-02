#include "dcc.h"

static FILE *outputFile = NULL;

static char            **inc = NULL;
static definitionList_t *def = NULL;

static bool preprocessFile(FILE *inputFile, char *inputFileName);

static bool doInclude(char *buffer, int bufferSize, int *tokenStartIndex, char *fileName, int line)
{
    char *token             = NULL;
    char *newFileName       = NULL;
    bool  isGlobal          = false;
    FILE *newInputFile      = NULL;
    char  cwdRestore[512]   = {0};
    bool  rc                = false;
    char *singleIncludeName = NULL;

    if (NULL == buffer          ||
        NULL == tokenStartIndex ||
        NULL == fileName)
    {
        INTERNAL_ERROR;
        return false;
    }

    token = getNextTokenFromBuf(buffer, bufferSize, true, tokenStartIndex);

    if (false == stringWrappedWith(token, '"') &&
        false == (token[0] == '<' &&
                  stringEndsWith(token, '>')))
    {
        ERROR(fileName, line, buffer, "#include expects \"FILENAME\" or <FILENAME>");
        return false;
    }

    isGlobal = token[0] == '<';

    // Remove "" or <>
    newFileName = strncpy(calloc(strlen(token) - 1, sizeof(char)),
                          &(token[1]),
                          strlen(token) - 2);

    free(token);

    token = getNextTokenFromBuf(buffer, bufferSize, false, tokenStartIndex);

    if (NULL != token)
    {
        ERROR(fileName, line, buffer, "extra tokens at end of #include directive");
        free(token);
        free(fileName);
        return false;
    }

    if (isGlobal)
    {
        // Global include
        if (cwdRestore != getcwd(cwdRestore, sizeof(cwdRestore)))
        {
            INTERNAL_ERROR;
            return false;
        }

        if (0 != chdir(STD_INCLUDE_PATH))
        {
            printf("Warning: cannot open %s\n", STD_INCLUDE_PATH);
        }
        else
        {
            newInputFile = fopen(newFileName, "r");

            if (NULL != newInputFile)
            {
                goto fileFound;
            }
        }

        if (0 != chdir(LIB_INCLUDE_PATH))
        {
            printf("Warning: cannot open %s\n", LIB_INCLUDE_PATH);
        }
        else
        {
            newInputFile = fopen(newFileName, "r");

            if (NULL != newInputFile)
            {
                goto fileFound;
            }
        }

        chdir(cwdRestore);
    }
    else
    {
        // Local Include
        newInputFile = fopen(newFileName, "r");

        if (NULL != newInputFile)
        {
            goto fileFound;
        }

        if (inc != NULL)
        {
            for (int i = 0; NULL != inc[i]; i++)
            {
                if (false == isolateFileNameWithExtension(inc[i],
                                                          &singleIncludeName) ||
                    NULL  == singleIncludeName)
                {
                    INTERNAL_ERROR;
                    continue;
                }

                if (0 == strcmp(singleIncludeName, newFileName))
                {
                    free(singleIncludeName);
                    newInputFile = fopen(inc[i], "r");

                    if (NULL == newInputFile)
                    {
                        ERROR_ARGS(fileName, line, buffer,
                                "could not open %s", inc[i]);
                        free(newFileName);
                        return false;
                    }

                    goto fileFound;
                }

                free(singleIncludeName);

            }
        }
    }

    ERROR_ARGS(fileName, line, buffer, "could not find %s", newFileName);
    free(newFileName);

    return false;

    fileFound:
    rc = preprocessFile(newInputFile, newFileName);
    if ('\0' != cwdRestore[0])
    {
        chdir(cwdRestore);
    }
    fclose(newInputFile);
    free(newFileName);
    return rc;
}

static bool doDefine(char *buffer, int bufferSize, int *tokenStartIndex, char *fileName, int line)
{
    char         *token          = NULL;
    definition_t *newDef         = NULL;
    int           endOfNameToken = 0;

    if (NULL == buffer          ||
        NULL == tokenStartIndex ||
        NULL == fileName)
    {
        INTERNAL_ERROR;
        return false;
    }

    token = getNextTokenFromBuf(buffer, bufferSize, false, tokenStartIndex);

    if (NULL == token)
    {
        ERROR(fileName, line, buffer, "no macro name given in #define directive");
        return false;
    }

    if (false == isCharForIdent(token[0]) ||
        ('0' <= token[0] && '9' >= token[0]))
    {
        ERROR(fileName, line, buffer, "expected an indentifier");
        free(token);
        return false;
    }

    if (NULL != getDefinition(def, token))
    {
        ERROR_ARGS(fileName, line, buffer, "\"%s\" redefined", token);
        free(token);
        return false;
    }

    newDef = addDefinition(def);

    if (NULL == newDef)
    {
        INTERNAL_ERROR;
        free(token);
        return false;
    }

    newDef->name = token;

    endOfNameToken = *tokenStartIndex;

    token = getNextTokenFromBuf(buffer, bufferSize, false, tokenStartIndex);

    if (NULL == token)
    {
        // Empty definition, no problem with that!
        return true;
    }

    // We don't actually ant the token, we just used it as an indicator of
    // whether or not there was anything to define
    free(token);

    while (buffer[endOfNameToken] == ' ' ||
           buffer[endOfNameToken] == '\t')
    {
        endOfNameToken++;
    }

    // Copy everything else except for the newline at the end of the buffer
    newDef->expansion = strncpy(calloc(strnlen(&(buffer[endOfNameToken]),
                                               bufferSize - endOfNameToken - 1),
                                       sizeof(char)),
                        &(buffer[endOfNameToken]),
                       strnlen(&(buffer[endOfNameToken]),
                               bufferSize - endOfNameToken - 1) - 1);

    return true;
}

static bool preprocessFile(FILE *inputFile, char *inputFileName)
{
    char          inputBuffer[2048] = {0};
    int           lineNumber        = 0;
    int           tokenStartIndex   = 0;
    char         *token             = NULL;
    bool          success           = true;
    definition_t *searchDef         = NULL;

    while(fgets(inputBuffer, sizeof(inputBuffer), inputFile))
    {
        lineNumber++;

        tokenStartIndex = 0;

        token = getNextTokenFromBuf(inputBuffer, sizeof(inputBuffer), false, &tokenStartIndex);

        if (NULL == token)
        {
            fprintf(outputFile, "\n");
            continue;
        }

        if (0 == strcmp(token, PREPRO_DIRECTIVE_PREFIX))
        {
            // Preprocessor directive!

            free(token);
            token = getNextTokenFromBuf(inputBuffer,
                                        sizeof(inputBuffer),
                                        false,
                                        &tokenStartIndex);

            if (0 == strcmp(PREPRO_DIRECTIVE_INCLUDE, token))
            {
                free(token);
                if (false == doInclude(inputBuffer,
                                       sizeof(inputBuffer),
                                       &tokenStartIndex,
                                       inputFileName,
                                       lineNumber))
                {
                    success = false;
                }
            }
            else if (0 == strcmp(PREPRO_DIRECTIVE_DEFINE, token))
            {
                free(token);
                if (false == doDefine(inputBuffer,
                                      sizeof(inputBuffer),
                                      &tokenStartIndex,
                                      inputFileName,
                                      lineNumber))
                {
                    success = false;
                }

                fprintf(outputFile, "\n");
            }
            else
            {
                ERROR_ARGS(inputFileName,
                           lineNumber,
                           inputBuffer,
                           "unknown preprocessor directive \"%s\"",
                           token);
                free(token);
                success = false;
            }

            continue;
        }

        free(token);

        tokenStartIndex = 0;

        while (NULL != (token = getNextTokenFromBuf(inputBuffer,
                                                    sizeof(inputBuffer),
                                                    false,
                                                    &tokenStartIndex)))
        {
            if (0 == strcmp("//", token))
            {
                free(token);
                break;
            }

            if (isCharForIdent(token[0]))
            {
                if (isCharForIdentNoNum(token[0]))
                {
                    searchDef = getDefinition(def, token);

                    if (NULL != searchDef)
                    {
                        fprintf(outputFile, "%s ", searchDef->expansion);
                        continue;
                    }
                }

                fprintf(outputFile, "%s ", token);
                continue;
            }

            // Just dump everything else
            fprintf(outputFile, "%s ", token);

            free(token);
        }

        fprintf(outputFile, "\n");
    }

    return success;
}

bool preprocessor(char *inName, char *outName, char **includes)
{
    definitionList_t definitions = {0};
    FILE            *inputFile   = NULL;
    bool             success     = false;

    if (NULL == inName  ||
        NULL == outName ||
        NULL == includes)
    {
        INTERNAL_ERROR;
        return false;
    }

    inc = includes;
    def = &definitions;

    inputFile = fopen(inName, "r");

    if (NULL == inputFile)
    {
        printf("Could not open %s\n", inName);
        return false;
    }

    outputFile = fopen(outName, "w");

    if (NULL == outputFile)
    {
        fclose(inputFile);
        INTERNAL_ERROR;
        return false;
    }

    success = preprocessFile(inputFile, inName);

    freeDefinitionListContents(def);

    fclose(inputFile);
    fclose(outputFile);

    if (false == success)
    {
        remove(outName);
    }

    return success;
}
