#include "dcc.h"

int main()
{
    char *dirIncludes[] =
    {
        "."
    };

    includes_t i =
    {
        .dirIncludeCount    = 1,
        .dirIncludes        = (char**)&dirIncludes,
        .singleIncludeCount = 0,
        .singleIncludes     = NULL
    };

    (void)preprocessor("test.c", "test.di", &i);

    return 0;
}
