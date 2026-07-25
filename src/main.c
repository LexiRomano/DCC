#include "dcc.h"

static stringLinkedList_t src     = {0};
static char             **inc     = NULL;
static bool               keepTmp = false;            

static void printHelp()
{
    printf("Usage: dcc file... [options]\n");
    printf("Options:\n");
    printf("  --help         Displays this information\n");
    printf("  -I             All following files are included instead of compiled\n");
    printf("  -k             Keep all temporary files\n");
}

// Returns -1 on error, 0 on graceful
// exit, 1 on successful parse
static int parseArgs(int argc, char *argv[])
{
    stringLinkedList_t includes = {0};
    strll_t           *tmpStrll = NULL;
    bool               dashI    = false;

    if (NULL == argv ||
        0    == argc)
    {
        return false;
    }

    if (1 == argc)
    {
        // help
        printHelp();
        return 0;
    }

    // Start at 1 to skip our own invocation
    for (int i = 1; i < argc; i++)
    {
        if (NULL == argv[i])
        {
            continue;
        }

        if ('-' == argv[i][0])
        {
            // some sort of option
            if (0 == strcmp(argv[i], "--help"))
            {
                printHelp();
                freeStringLinkedListContents(&src);
                freeStringLinkedListContents(&includes);
                return 0;
            }
            else if (0 == strcmp(argv[i], "-I"))
            {
                dashI = true;
            }
            else if (0 == strcmp(argv[i], "-k"))
            {
                keepTmp = true;
            }
            else
            {
                printf("Unknown flag \"%s\"\n", argv[i]);
                freeStringLinkedListContents(&src);
                freeStringLinkedListContents(&includes);
                return -1;
            }
            continue;
        }

        if (dashI)
        {
            // Include
            tmpStrll = addStringLinkedList(&includes);

            if (NULL == tmpStrll)
            {
                INTERNAL_ERROR;
                freeStringLinkedListContents(&src);
                freeStringLinkedListContents(&includes);
                return -1;
            }

            tmpStrll->str = strcpy(calloc(strlen(argv[i]) + 1, sizeof(char)),
                                   argv[i]);
        }
        else
        {
            // Compile
            tmpStrll = addStringLinkedList(&src);

            if (NULL == tmpStrll)
            {
                INTERNAL_ERROR;
                freeStringLinkedListContents(&src);
                freeStringLinkedListContents(&includes);
                return -1;
            }

            tmpStrll->str = strcpy(calloc(strlen(argv[i]) + 1, sizeof(char)),
                                   argv[i]);
        }

    }

    inc = calloc(includes.count + 1, sizeof(char*));

    tmpStrll = includes.first;
    for (int i = 0; i < includes.count; i++)
    {
        inc[i] = strcpy(calloc(strlen(tmpStrll->str) + 1, sizeof(char)),
                        tmpStrll->str);

        tmpStrll = tmpStrll->next;
    }

    inc[includes.count] = NULL;

    freeStringLinkedListContents(&includes);

    return 1;
}

static bool createTmpDirectory()
{
    DIR *dirp;

    dirp = opendir(TEMP_DIRECTORY);

    if (NULL == dirp)
    {
        if (0 == mkdir(TEMP_DIRECTORY,
                       (S_IRWXU | S_IRWXG | S_IROTH))) // 774 permission
        {
            return true;
        }

        return false;
    }

    closedir(dirp);

    return true;
}

static void removeTmpDirectory()
{
    char command[64] = {0};

    snprintf(command, sizeof(command), "rm -rf %s", TEMP_DIRECTORY);

    system(command);
}

int main(int argc, char *argv[])
{
    int   rc               = 0;
    char *fileName         = NULL;
    char  preproName[256]  = {0};
    char  dsbName[256]     = {0};
    bool  success          = true;

    rc = parseArgs(argc, argv);

    if (rc <= 0)
    {
        return rc;
    }

    if (false == createTmpDirectory())
    {
        success = false;
        goto fail;
    }

    for (strll_t *s = src.first; s != NULL; s = s->next)
    {
        if (false == isolateFileName(s->str, &fileName))
        {
            INTERNAL_ERROR;
            success = false;
            goto fail;
        }

        snprintf(preproName, sizeof(preproName), "%s/%s.%s",
                 TEMP_DIRECTORY, fileName, PREPRO_FILE_EXTENSION);

        snprintf(dsbName, sizeof(dsbName), "%s/%s.%s",
                 TEMP_DIRECTORY, fileName, DSB_FILE_EXTENSION);

        free(fileName);

        if (false == preprocessor(s->str, preproName, inc) ||
            false == compile(preproName, dsbName))
        {
            success = false;
            continue;
        }
    }

    if (false == keepTmp)
    {
        removeTmpDirectory();
    }

fail:
    freeStringLinkedListContents(&src);
    for (int i = 0; inc[i] != NULL; i++)
    {
        free(inc[i]);
    }
    free(inc);

    return success ? 0: -1;
}
