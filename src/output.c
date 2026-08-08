#include "dcc.h"

static FILE         *outputFile = NULL;
static parsedData_t *pd         = NULL;

static bool outputGlobalVariables()
{
    FILE *globalFile = NULL;

    globalFile = fopen(GLOBALS_DSB_FILE, "a");

    if (NULL == globalFile)
    {
        INTERNAL_ERROR;
        return false;
    }

    for (variable_t *v = pd->globalVariables.first; NULL != v; v = v->next)
    {
        fprintf(globalFile, "    .export  %s\n", v->identifier);
        fprintf(globalFile, "    .reserve %u\n", v->type.size);
    }

    fclose(globalFile);

    return true;
}

static bool outputExpression(expression_t *exp)
{
    // TODO
    fprintf(outputFile, "    //TODO: expressions\n");
    return true;
}

// Forward declaration
static bool outputStackFrame(stackFrame_t *sf);

static bool outputIf(if_t *ifData)
{
    unsigned int endId   = 0;
    unsigned int skipId  = 0;
    bool         started = false;

    if (NULL == ifData)
    {
        INTERNAL_ERROR;
        return false;
    }

    endId = ftell(outputFile);

    while (ifData != NULL)
    {
        if (started)
        {
            fprintf(outputFile, "    :__skipIf_%u__\n", skipId);
        }
        started = true;

        if (NULL != ifData->condition)
        {
            if (NULL != ifData->next)
            {
                skipId = ftell(outputFile);
            }

            outputExpression(ifData->condition);
            fprintf(outputFile, "    COMP          G0 0\n");
            fprintf(outputFile, "    BREQ          ");
            if (NULL == ifData->next)
            {
                fprintf(outputFile, "__doneIf_%u__\n", endId);
            }
            else
            {
                fprintf(outputFile, "__skipIf_%u__\n", skipId);
            }
        }

        outputStackFrame(&ifData->consequence);

        if (NULL != ifData->next)
        {
            fprintf(outputFile, "    BRAL          __doneIf_%u__\n", endId);
        }

        ifData = ifData->next;
    }

    fprintf(outputFile, "    :__doneIf_%u__\n", endId);
    return true;
}

static bool outputStackFrame(stackFrame_t *sf)
{
    if (NULL == sf)
    {
        INTERNAL_ERROR;
        return false;
    }

    // Align SP
    if (sf->varSize > 0)
    {
        fprintf(outputFile, "    ADD           SP SP %u\n", sf->varSize);
    }

    for (voidContainer_t *vc = sf->codeBlock.firstItem; vc != NULL; vc = vc->nextVoidContainer)
    {
        switch (vc->type)
        {
            case stackFrame_e:
            {
                if (false == outputStackFrame(vc->data))
                {
                    return false;
                }

                break;
            }
            case expression_e:
            {
                if (false == outputExpression(vc->data))
                {
                    return false;
                }
                break;
            }
            case if_e:
            {
                if (false == outputIf(vc->data))
                {
                    return false;
                }
            }
            default:
            {
                continue;
            }
        }
    }

    // Restore SP
    if (sf->varSize > 0 && NULL != sf->prevAccessableStackFrame)
    {
        // If the previous stack frame is NULL, we're in the first frame
        // of the function. In that case, the return routine will
        // deal with the SP. Also if this is a non-void function, this
        // should be inaccessable due to a return being required before
        // hitting the end of the root stack frame.
        fprintf(outputFile, "    SUB           SP SP %u\n", sf->varSize);
    }

    return true;
}

static bool outputFunctions()
{
    for (function_t *f = pd->functions.first; NULL != f; f = f->next)
    {
        if (false == f->isDefined)
        {
            continue;
        }

        fprintf(outputFile, "\n.section %s\n", f->identifier);

        // Declare required symbols
        fprintf(outputFile, "    .requires __STACK_OVERFLOW__\n");
        for (strll_t *s = f->requiredSymbols.first; NULL != s; s = s->next)
        {
            fprintf(outputFile, "    .requires %s\n", s->str);
        }

        // Check for stack overflow
        fprintf(outputFile, "    ADD           G0 SP %u\n", f->definition.maxStackSize);
        fprintf(outputFile, "    COMP          G0 0xFFC0\n");
        fprintf(outputFile, "    BRHS          __STACK_OVERFLOW__\n");

        if (false == outputStackFrame(&f->definition))
        {
            return false;
        }

        // Return routine
        fprintf(outputFile, "    :ret\n");
        fprintf(outputFile, "    MOVE          SP OB\n");
        fprintf(outputFile, "    RETURN\n");
    }
    return true;
}

bool output(parsedData_t *parsedData, char *dsbFileName)
{
    bool rc = true;

    if (NULL == parsedData)
    {
        return false;
    }

    pd = parsedData;

    if (NULL == dsbFileName)
    {
        INTERNAL_ERROR;
        freeParsedDataContents(parsedData);
        free(parsedData);
        return false;
    }

    outputFile = fopen(dsbFileName, "w");

    if (NULL == outputFile)
    {
        INTERNAL_ERROR;
        freeParsedDataContents(parsedData);
        free(parsedData);
        return false;
    }

    if (false == outputGlobalVariables() ||
        false == outputFunctions())
    {
        rc = false;
    }

    freeParsedDataContents(parsedData);
    free(parsedData);

    fclose(outputFile);

    if (false == rc)
    {
        remove(dsbFileName);
    }

    return rc;
}
