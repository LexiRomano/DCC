#ifndef __DCC_H__
#define __DCC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "parsedStructs.h"

#define INTERNAL_ERROR printf("Internal error: %s:%d\n", __FUNCTION__, __LINE__)

// buf is expected to contain a newline
#define ERROR(file, line, buf, msg)           printf("%s:%d: error: " msg "\n  %d | %s", file, line, line, buf)
#define ERROR_ARGS(file, line, buf, msg, ...) printf("%s:%d: error: " msg "\n  %d | %s", file, line, __VA_ARGS__, line, buf)

#define TEMP_DIRECTORY "dTemp"

#define PREPRO_FILE_EXTENSION "di"
#define DSB_FILE_EXTENSION    "dsb"
#define STD_INCLUDE_PATH "/usr/lib/dcc/include"
#define LIB_INCLUDE_PATH "/usr/lib/dlib/include"

#define PREPRO_DIRECTIVE_PREFIX  "#"
#define PREPRO_DIRECTIVE_INCLUDE "include"
#define PREPRO_DIRECTIVE_DEFINE  "define"

bool preprocessor(char *inName, char *outName, char **includes);
bool compile(char *inputFileName, char *outputFilenNme);

extern char *g_keywords[];

// Util
int  findFirstNonWhitespace(char *buf, int length);
bool isCharForIdent(char c);
bool isCharForIdentNoNum(char c);
char *getNextTokenFromBuf(char *buf, int bufSize, bool tokenizeAngleBrackets, int *startIndex);
char *getNextTokenFromFile(FILE *file, int *lineNumber);
bool isolateFileName(char *in, char **out);
bool isolateFileNameWithExtension(char *in, char **out);
bool stringStartsWith(char *str, char c);
bool stringEndsWith(char *str, char c);
bool stringWrappedWith(char *str, char c);
char *getCurrentLine(FILE *file);
bool isKeyword(char *str);
bool isIdentifier(char *str);

definition_t *addDefinition(definitionList_t *list);
definition_t *getDefinition(definitionList_t *list, char *name);
void freeDefinitionListContents(definitionList_t *list);

strll_t *addStringLinkedList(stringLinkedList_t *list);
void freeStringLinkedListContents(stringLinkedList_t *list);

void freeFunctionContents(function_t *func);

variable_t *findVariable(variableList_t *varList, char *ident);
void freeVariableList(variableList_t *varList);

void freeVoidListContents(voidList_t *vl);

#endif //__DCC_H__
