#ifndef __DCC_H__
#define __DCC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define INTERNAL_ERROR printf("Internal error: %s:%d\n", __FUNCTION__, __LINE__)

#define ERROR(file, line, buf, msg)           printf("%s:%d: error: " msg "\n  %d | %s\n\n", file, line, line, buf)
#define ERROR_ARGS(file, line, buf, msg, ...) printf("%s:%d: error: " msg "\n  %d | %s\n\n", file, line, __VA_ARGS__, line, buf)

#define TEMP_DIRECTORY "dTemp"

#define PREPRO_FILE_EXTENSION ".di"
#define STD_INCLUDE_PATH "/usr/lib/dcc/include"
#define LIB_INCLUDE_PATH "/usr/lib/dlib/include"

#define PREPRO_DIRECTIVE_PREFIX  "#"
#define PREPRO_DIRECTIVE_INCLUDE "include"
#define PREPRO_DIRECTIVE_DEFINE  "define"

typedef struct
{
    char **singleIncludes;
    int    singleIncludeCount;
    char **dirIncludes;
    int    dirIncludeCount;
} includes_t;

typedef struct definition_t
{
    char                *name;
    char                *expansion;
    struct definition_t *next;
} definition_t;

typedef struct
{
    definition_t *first;
    definition_t *last;
} definitionList_t;

bool preprocessor(char *inName, char *outName, includes_t *includes);

// Util
int  findFirstNonWhitespace(char *buf, int length);
bool isCharForIdent(char c);
bool isCharForIdentNoNum(char c);
char *getNextTokenFromBuf(char *buf, int bufSize, bool tokenizeAngleBrackets, int *startIndex);
bool isolateFileName(char *in, char **out);
bool isolateFileNameWithExtension(char *in, char **out);
bool stringStartsWith(char *str, char c);
bool stringEndsWith(char *str, char c);
bool stringWrappedWith(char *str, char c);

definition_t *addDefinition(definitionList_t *list);
definition_t *getDefinition(definitionList_t *list, char *name);

#endif //__DCC_H__
