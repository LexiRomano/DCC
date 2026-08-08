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

    if (false == outputGlobalVariables())
    {
        rc = false;
    }

    freeParsedDataContents(parsedData);
    free(parsedData);

    fclose(outputFile);

    return rc;
}
