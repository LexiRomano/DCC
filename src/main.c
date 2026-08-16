#include "dcc.h"

static stringLinkedList_t src     = {0};
static char             **inc     = NULL;
static bool               keepTmp = false;
static bool               objOut  = false;

static stringLinkedList_t configSource   = {0};
static stringLinkedList_t configObject   = {0};
static stringLinkedList_t configSection  = {0};
static bool               startIsDefined = false;

static void printHelp()
{
    printf("Usage: dcc file ... [options]\n");
    printf("Options:\n");
    printf("  --help         Displays this information\n");
    printf("  -I             All following files are included instead of compiled\n");
    printf("  -O             Output an object file\n");
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
            else if (0 == strcmp(argv[i], "-O"))
            {
                objOut = true;
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

    if (objOut && src.count > 1)
    {
        printf("Error: can only output object files for one source file at a time\n");
        freeStringLinkedListContents(&src);
        freeStringLinkedListContents(&includes);
        return -1;
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

static bool prepareGlobals()
{
    FILE *globalFile = NULL;

    globalFile = fopen(GLOBALS_DSB_FILE, "w");

    if (NULL == globalFile)
    {
        INTERNAL_ERROR;
        return false;
    }

    fprintf(globalFile, ".section __GLOBALS__\n");

    fclose(globalFile);

    return true;
}

static bool finalizeGlobals()
{
    FILE *globalFile = NULL;

    globalFile = fopen(GLOBALS_DSB_FILE, "a");

    if (NULL == globalFile)
    {
        INTERNAL_ERROR;
        return false;
    }

    fprintf(globalFile, "    .export __STACK_SPACE_START__\n");

    fclose(globalFile);

    return true;
}

static bool prepareConfig()
{
    FILE *configFile = NULL;

    configFile = fopen(DFG_FILE, "w");

    if (NULL == configFile)
    {
        INTERNAL_ERROR;
        return false;
    }

    fprintf(configFile, ".out    " OUTPUT_FILE_NAME "\n");

    for (strll_t *s = configSource.first; NULL != s; s = s->next)
    {
        fprintf(configFile, ".source %s\n", s->str);
    }

    fprintf(configFile, ".source %s\n", GLOBALS_DSB_FILE);

    if (false == startIsDefined)
    {
        fprintf(configFile, ".object " STD_OBJECTS_PATH "/__std_start__.dob\n");
    }
    fprintf(configFile, ".object " STD_OBJECTS_PATH "/__std_core__.dob\n");

    for (strll_t *s = configObject.first; NULL != s; s = s->next)
    {
        fprintf(configFile, ".object %s\n", s->str);
    }

    fprintf(configFile, "\n_start\n");

    for (strll_t *s = configSection.first; NULL != s; s = s->next)
    {
        fprintf(configFile, "%s\n", s->str);
    }

    fprintf(configFile, "__std_core__\n");
    fprintf(configFile, "%s\n", GLOBALS_SECTION_NAME);

    fclose(configFile);

    return true;
}

static bool addToSourceInternal(stringLinkedList_t *strll, char *str)
{
    strll_t *newStrll = NULL;

    if (NULL == strll ||
        NULL == str)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (NULL != findStringLinkedList(strll, str))
    {
        return true;
    }

    newStrll = addStringLinkedList(strll);

    if (NULL == newStrll)
    {
        ALLOC_ERROR;
        return false;
    }

    newStrll->str = strdup(str);

    return true;
}

static bool copyObjectFileOut()
{
    char *fileName = NULL;
    char  buf[256] = {0};

    if (NULL == src.first ||
        false == isolateFileName(src.first->str, &fileName))
    {
        INTERNAL_ERROR;
        return false;
    }

    snprintf(buf, sizeof(buf), "cp %s/%s.%s %s.%s", TEMP_DIRECTORY, fileName, DOB_FILE_EXTENSION,
                fileName, DOB_FILE_EXTENSION);

    free(fileName);

    if (0 != system(buf))
    {
        return false;
    }

    return true;
}

bool addSourceToConfig(char *src)
{
    return addToSourceInternal(&configSource, src);
}

bool addObjectToConfig(char *obj)
{
    return addToSourceInternal(&configObject, obj);
}

bool addSectionToConfig(char *sec)
{
    return addToSourceInternal(&configSection, sec);
}

void startDefined()
{
    startIsDefined = true;
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

    if (false == createTmpDirectory() ||
        false == prepareGlobals())
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
            false == output(parse(preproName), dsbName))
        {
            success = false;
            continue;
        }
    }

    if (success)
    {

        if (false == finalizeGlobals() ||
            false == prepareConfig())
        {
            success = false;
            goto fail;
        }
        
        if (objOut)
        {
            if (0     != system("dssembly -o " DFG_FILE) ||
                false == copyObjectFileOut())
            {
                success = false;
            }
        }
        else
        {
            if (0 != system("dssembly -k " DFG_FILE))
            {
                remove(OUTPUT_FILE_NAME);
                success = false;
            }
        }
    }
        

    if (false == keepTmp)
    {
        removeTmpDirectory();
    }

fail:
    freeStringLinkedListContents(&src);
    freeStringLinkedListContents(&configSource);
    freeStringLinkedListContents(&configObject);
    freeStringLinkedListContents(&configSection);
    for (int i = 0; inc[i] != NULL; i++)
    {
        free(inc[i]);
    }
    free(inc);

    return success ? 0: -1;
}
