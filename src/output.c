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

        // Align SP
        if (f->definition.varSize > 0)
        {
            fprintf(outputFile, "    ADD           SP SP %u\n", f->definition.varSize);
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

    return rc;
}
