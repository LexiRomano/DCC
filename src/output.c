#include "dcc.h"

bool output(parsedData_t *parsedData)
{
    if (NULL == parsedData)
    {
        return false;
    }

    // TODO magic

    freeParsedDataContents(parsedData);
    free(parsedData);

    return true;
}
