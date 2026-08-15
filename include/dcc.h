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
#include <stdint.h>

#include "parsedStructs.h"
#include "dsbUtils.h"

#define INTERNAL_ERROR printf("Internal error: %s:%d\n", __FUNCTION__, __LINE__)
#define ALLOC_ERROR printf("Memory allocation failure\n")

// buf is expected to contain a newline
#define ERROR(file, line, buf, msg)           printf("%s:%d: error: " msg "\n  %d | %s", file, line, line, buf)
#define ERROR_ARGS(file, line, buf, msg, ...) printf("%s:%d: error: " msg "\n  %d | %s", file, line, __VA_ARGS__, line, buf)

#define GLOBALS_SECTION_NAME "__GLOBALS__"

#define TEMP_DIRECTORY   "dTemp"
#define GLOBALS_DSB_FILE "dTemp/" GLOBALS_SECTION_NAME ".dsb"
#define DFG_FILE         "dTemp/config.dfg"

#define OUTPUT_FILE_NAME "out.bin"

#define PREPRO_FILE_EXTENSION "di"
#define DSB_FILE_EXTENSION    "dsb"
#define STD_INCLUDE_PATH "/usr/lib/dcc/include"
#define LIB_INCLUDE_PATH "/usr/lib/dlib/include"

#define PREPRO_DIRECTIVE_PREFIX  "#"
#define PREPRO_DIRECTIVE_INCLUDE "include"
#define PREPRO_DIRECTIVE_DEFINE  "define"

bool preprocessor(char *inName, char *outName, char **includes);
parsedData_t *parse(char *inputFileName);
bool output(parsedData_t *parsedData, char *dsbFileName);

bool addSourceToConfig(char *src);
bool addObjectToConfig(char *obj);
bool addSectionToConfig(char *sec);

extern char *g_keywords[];

// Util
int  findFirstNonWhitespace(char *buf, int length);
bool isCharForIdent(char c);
bool isCharForIdentNoNum(char c);
char *getNextTokenFromBuf(char *buf, int bufSize, bool tokenizeAngleBrackets, int *startIndex);
char *getNextTokenFromFile(FILE *file, int *lineNumber);
char *peekNextTokenFromFile(FILE *file);
bool isolateFileName(char *in, char **out);
bool isolateFileNameWithExtension(char *in, char **out);
bool stringStartsWith(char *str, char c);
bool stringEndsWith(char *str, char c);
bool stringWrappedWith(char *str, char c);
bool stripQuotes(char *str);
bool proccessEscapeCharacters(char *str);
char *getCurrentLine(FILE *file);
bool isKeyword(char *str);
bool isIdentifier(char *str);
bool parseLiteral(char *literal, unsigned int *out);
bool isTypeSigned(type_t *t);

definition_t *addDefinition(definitionList_t *list);
definition_t *getDefinition(definitionList_t *list, char *name);
void freeDefinitionListContents(definitionList_t *list);

strll_t *addStringLinkedList(stringLinkedList_t *list);
strll_t *findStringLinkedList(stringLinkedList_t *list, char *target);
void freeStringLinkedListContents(stringLinkedList_t *list);

void freeFunctionContents(function_t *func);
void freeFunctionListContents(functionList_t *fl);

variable_t *findVariable(variableList_t *varList, char *ident);
void freeVariableListContents(variableList_t *varList);

voidContainer_t *addVoidContainer(voidList_t *vl);
void freeVoidListContents(voidList_t *vl);

void freeExpression(expression_t **exp);

void freeParsedDataContents(parsedData_t *p);

function_t *findFunction(functionList_t *fl, char *ident);

#endif //__DCC_H__
