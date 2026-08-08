#include "dcc.h"

// Must all be 2 char long
static const char *specialTokens[] = {
    "++",
    "--",
    "<<",
    ">>",
    "==",
    "!=",
    ">=",
    "<=",
    "&&",
    "||",
    "->",
    "//",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    NULL
};

int findFirstNonWhitespace(char *buf, int length)
{
    if (NULL == buf)
    {
        return -1;
    }

    for (int i = 0; i < length; i++)
    {
        if (' '  == buf[i] ||
            '\t' == buf[i])
        {
            continue;
        }

        if ('\n' == buf[i] ||
            '\0' == buf[i])
        {
            return -1;
        }

        return i;
    }

    return -1;
}

static inline bool isCharNumber(char c)
{
    return '0' <= c && '9' >= c;
}

bool isCharForIdent(char c)
{
    if ('a' <= c &&
        'z' >= c)
    {
        return true;
    }

    if ('A' <= c &&
        'Z' >= c)
    {
        return true;
    }

    if ('_' == c)
    {
        return true;
    }

    return isCharNumber(c);
}

bool isCharForIdentNoNum(char c)
{
    if ('a' <= c &&
        'z' >= c)
    {
        return true;
    }

    if ('A' <= c &&
        'Z' >= c)
    {
        return true;
    }

    return '_' == c;
}

static inline bool isCharWhitespace(char c)
{
    return ' '  == c ||
           '\t' == c ||
           '\n' == c ||
           '\r' == c;

}

char *getNextTokenFromBuf(char *buf, int bufSize, bool tokenizeAngleBrackets, int *startIndex)
{
    bool foundStart       = false;
    int  start            = 0;
    int  end              = 0;
    bool isInDoubleQuote  = false;
    bool isInSingleQuote  = false;
    bool isInAngleBracket = false;
    bool isInEscape       = false;

    if (NULL == buf        ||
        NULL == startIndex ||
        bufSize <= *startIndex)
    {
        return NULL;
    }

    start = *startIndex;

    end = -1;

    for (int i = start; i < bufSize; i++)
    {
        if (false == foundStart)
        {
            if ('\n' == buf[i] ||
                '\r' == buf[i] ||
                '\0' == buf[i])
            {
                break;
            }

            if (isCharWhitespace(buf[i]))
            {
                start++;
                continue;
            }

            foundStart = true;
        }
        else if ('\n' == buf[i] ||
                 '\r' == buf[i] ||
                 '\0' == buf[i])
        {
            end = i - 1;
            break;
        }
        else if (false == isInDoubleQuote  &&
                 false == isInSingleQuote  &&
                 false == isInAngleBracket &&
                 isCharWhitespace(buf[i]))
        {
            end = i - 1;
            break;
        }

        if (i == start)
        {
            if ('"' == buf[i])
            {
                isInDoubleQuote = true;
            }
            else if ('\'' == buf[i])
            {
                isInSingleQuote = true;
            }
            else if ('<' == buf[i] &&
                     tokenizeAngleBrackets)
            {
                isInAngleBracket = true;
            }
            else if (false == isCharForIdent(buf[i]))
            {
                end = i;
                for (int j = 0; NULL != specialTokens[j]; j++)
                {
                    if (specialTokens[j][0] == buf[i] &&
                        specialTokens[j][1] == buf[i + 1])
                    {
                        end = i + 1;
                        break;
                    }
                }
                break;
            }

            continue;
        }

        if ((isInDoubleQuote ||
             isInSingleQuote ||
             isInAngleBracket) &&
            '\\' == buf[i]    &&
             false == isInEscape)
        {
            isInEscape = true;
            continue;
        }

        if (false == isInEscape)
        {
            if (isInDoubleQuote)
            {
                if ('"' == buf[i])
                {
                    end = i;
                    break;
                }
            }
            else if (isInSingleQuote)
            {
                if ('\'' == buf[i])
                {
                    end = i;
                    break;
                }

            }
            else if (isInAngleBracket)
            {
                if ('>' == buf[i])
                {
                    end = i;
                    break;
                }

            }
        }

        isInEscape = false;

        if (isInDoubleQuote ||
            isInSingleQuote ||
            isInAngleBracket)
        {
            continue;
        }

        if (i != start &&
            false == isCharForIdent(buf[i]))
        {
            end = i - 1;
            break;
        }
    }

    if (-1 == end)
    {
        return NULL;
    }

    *startIndex = end + 1;

    if (start == bufSize)
    {
        return NULL;
    }

    return strncpy(calloc(end - start + 1, sizeof(char)), &buf[start], end - start + 1);
}

char *getNextTokenFromFile(FILE *file, int *lineNumber)
{
    char outputBuffer[1024] = {0};
    int  readValue          = 0;
    char currentChar        = '\0';
    bool foundStart         = false;
    bool isInDoubleQuote    = false;
    bool isInSingleQuote    = false;
    bool isInEscape         = false;

    if (NULL == file ||
        NULL == lineNumber)
    {
        INTERNAL_ERROR;
        return NULL;
    }

    for (int i = 0; i < sizeof(outputBuffer);)
    {
        readValue = fgetc(file);

        if (EOF == readValue)
        {
            break;
        }

        currentChar = (char)readValue;

        if (false == foundStart)
        {
            if ('\n' == currentChar)
            {
                (*lineNumber)++;
                continue;
            }

            if (isCharWhitespace(currentChar))
            {
                continue;
            }

            foundStart = true;
        }
        else if (false == isInDoubleQuote  &&
                 false == isInSingleQuote  &&
                 isCharWhitespace(currentChar))
        {
            if ('\n' == currentChar)
            {
                // Go back one so that the next pass
                // can catch the line number change
                fseek(file, -1, SEEK_CUR);
            }
            break;
        }

        if (0 == i)
        {
            if ('"' == currentChar)
            {
                isInDoubleQuote = true;
            }
            else if ('\'' == currentChar)
            {
                isInSingleQuote = true;
            }
            else if (false == isCharForIdent(currentChar))
            {
                outputBuffer[i++] = currentChar;
                readValue = fgetc(file);

                if (EOF == readValue)
                {
                    break;
                }

                fseek(file, -1, SEEK_CUR);
                for (int j = 0; NULL != specialTokens[j]; j++)
                {
                    if (specialTokens[j][0] == currentChar &&
                        specialTokens[j][1] == (char)readValue)
                    {
                        outputBuffer[i++] = (char)readValue;
                        fseek(file, 1, SEEK_CUR);
                        break;
                    }
                }
                break;
            }

            outputBuffer[i++] = currentChar;
            continue;
        }

        if ((isInDoubleQuote ||
             isInSingleQuote) &&
            '\\' == currentChar    &&
             false == isInEscape)
        {
            isInEscape = true;
            outputBuffer[i++] = currentChar;
            continue;
        }

        if (false == isInEscape)
        {
            if (isInDoubleQuote)
            {
                if ('"' == currentChar)
                {
                    outputBuffer[i] = currentChar;
                    break;
                }
            }
            else if (isInSingleQuote)
            {
                if ('\'' == currentChar)
                {
                    outputBuffer[i] = currentChar;
                    break;
                }

            }
        }

        isInEscape = false;

        if (isInDoubleQuote ||
            isInSingleQuote)
        {
            outputBuffer[i++] = currentChar;
            continue;
        }

        if (i != 0 &&
            false == isCharForIdent(currentChar))
        {
            outputBuffer[i++] = currentChar;
            fseek(file, -1, SEEK_CUR);
            break;
        }

        outputBuffer[i++] = currentChar;
    }

    if ('\0' == outputBuffer[0])
    {
        return NULL;
    }

    return strcpy(calloc(strlen(outputBuffer) + 1, sizeof(char)),
                  outputBuffer);
}

char *peekNextTokenFromFile(FILE *file)
{
    char *ret     = NULL;
    int   trash   = 0;
    int   restore = 0;

    restore = ftell(file);

    ret = getNextTokenFromFile(file, &trash);

    fseek(file, restore, SEEK_SET);

    return ret;
}

bool isolateFileName(char *in, char **out)
{
    int  rawNameStart = 0;
    int  rawNameStop  = 0;
    bool stopFound    = false;
    char rawName[256] = {0};

    if (NULL == in ||
        NULL == out)
    {
        INTERNAL_ERROR;
        return false;
    }

    for (int i = strlen(in) - 1; i != 0; i--)
    {
        if ('/' == in[i])
        {
            rawNameStart = i + 1;
            break;
        }

        if (false == stopFound &&
            '.'   == in[i])
        {
            rawNameStop = i;
            stopFound = true;
        }
    }

    if (false == stopFound)
    {
        rawNameStop = strlen(in);
    }

    if (rawNameStart == rawNameStop)
    {
        INTERNAL_ERROR;
        return false;
    }

    snprintf(rawName, rawNameStop - rawNameStart + 1, "%s", &(in[rawNameStart]));

    *out = strcpy(calloc(strlen(rawName) + 1, sizeof(char)), rawName);

    return true;
}

bool isolateFileNameWithExtension(char *in, char **out)
{
    int  rawNameStart = 0;
    char rawName[256] = {0};

    if (NULL == in ||
        NULL == out)
    {
        INTERNAL_ERROR;
        return false;
    }

    for (int i = strlen(in) - 1; i != 0; i--)
    {
        if ('/' == in[i])
        {
            rawNameStart = i + 1;
            break;
        }
    }

    if (rawNameStart == strlen(in))
    {
        INTERNAL_ERROR;
        return false;
    }

    snprintf(rawName, strlen(in) - rawNameStart + 1, "%s", &(in[rawNameStart]));

    *out = strcpy(calloc(strlen(rawName) + 1, sizeof(char)), rawName);

    return true;
}

bool stringStartsWith(char *str, char c)
{
    if (NULL == str)
    {
        return false;
    }

    return str[0] == c;
}

bool stringEndsWith(char *str, char c)
{
    if (NULL == str)
    {
        return false;
    }

    return str[strlen(str) - 1] == c;
}

bool stringWrappedWith(char *str, char c)
{
    if (NULL == str)
    {
        return false;
    }

    return stringStartsWith(str, c) && stringEndsWith(str, c);
}

definition_t *addDefinition(definitionList_t *list)
{
    if (NULL == list)
    {
        INTERNAL_ERROR;
        return NULL;
    }

    if (NULL == list->first)
    {
        list->first = calloc(1, sizeof(definition_t));
        list->last  = list->first;

        return list->last;
    }

    if (NULL == list->last)
    {
        INTERNAL_ERROR;
        return NULL;
    }

    list->last->next = calloc(1, sizeof(definition_t));
    list->last       = list->last->next;

    return list->last;
}

definition_t *getDefinition(definitionList_t *list, char *name)
{
    if (NULL == list ||
        NULL == name)
    {
        INTERNAL_ERROR;
        return NULL;
    }

    for (definition_t *d = list->first; d != NULL; d = d->next)
    {
        if (0 == strcmp(d->name, name))
        {
            return d;
        }
    }

    return NULL;
}

void freeDefinitionListContents(definitionList_t *list)
{
    definition_t *current = NULL;
    definition_t *next    = NULL;

    if (NULL == list ||
        NULL == list->first)
    {
        return;
    }

    while (current != NULL)
    {
        free(current->name);
        free(current->expansion);

        next = current->next;
        free(current);
        current = next;
    }

    list->first = NULL;
    list->last  = NULL;
}

strll_t *addStringLinkedList(stringLinkedList_t *list)
{
    if (NULL == list)
    {
        return NULL;
    }

    if (NULL == list->first)
    {
        list->first = calloc(1, sizeof(strll_t));
        list->last  = list->first;
        list->count = 1;

        return list->last;
    }

    if (NULL == list->last)
    {
        return NULL;
    }

    list->last->next = calloc(1, sizeof(strll_t));
    list->last       = list->last->next;
    list->count++;

    return list->last;
}

void freeStringLinkedListContents(stringLinkedList_t *list)
{
    strll_t *current = NULL;
    strll_t *next    = NULL;

    if (NULL == list ||
        NULL == list->first)
    {
        return;
    }

    while (current != NULL)
    {
        free(current->str);

        next = current->next;
        free(current);
        current = next;
    }

    list->first = NULL;
    list->last  = NULL;
    list->count = 0;
}

char *getCurrentLine(FILE *file)
{
    int  restore      = 0;
    char tmpChar      = 0;
    char buffer[1024] = {0};

    if (NULL == file)
    {
        return NULL;
    }

    restore = ftell(file);

    while (0 != ftell(file))
    {
        fseek(file, -1, SEEK_CUR);

        tmpChar = fgetc(file);

        if ('\n' == tmpChar)
        {
            break;
        }

        fseek(file, -1, SEEK_CUR);
    }

    fgets(buffer, sizeof(buffer), file);

    fseek(file, restore, SEEK_SET);

    return strcpy(calloc(strlen(buffer) + 1, sizeof(char)), buffer);
}

bool isKeyword(char *str)
{
    for (char **k = g_keywords; NULL != *k; k++)
    {
        if (0 == strcmp(str, *k))
        {
            return true;
        }
    }

    return false;
}

bool isIdentifier(char *str)
{
    if (NULL == str ||
        0    == strlen(str))
    {
        return false;
    }

    if (false == isCharForIdentNoNum(str[0]))
    {
        return false;
    }

    for (char *p = str + 1; '\0' != *p; p++)
    {
        if (false == isCharForIdent(*p))
        {
            return false;
        }
    }

    return !isKeyword(str);
}

static bool parseBinary(char *str, unsigned int *out)
{
    unsigned int sum = 0;
    unsigned int prevSum = 0;
    char        *pointer = str;

    if (NULL == str ||
        NULL == out)
    {
        INTERNAL_ERROR;
        return false;
    }

    while (*pointer != '\0')
    {
        prevSum = sum;
        sum *= 0b10;

        if (sum < prevSum)
        {
            return false;
        }

        switch (*pointer)
        {
            case '1':
                sum++;
            case '0':
                break;
            default:
                return false;
        }

        pointer++;
    }

    *out = sum;

    return true;
}

static bool parseDecimal(char *str, unsigned int *out)
{
    unsigned int sum     = 0;
    unsigned int prevSum = 0;
    char        *pointer = str;

    while (*pointer != '\0')
    {
        prevSum = sum;
        sum *= 10;
        if (sum < prevSum)
        {
            return false;
        }

        switch (*pointer)
        {
            case '9':
                sum++;
            case '8':
                sum++;
            case '7':
                sum++;
            case '6':
                sum++;
            case '5':
                sum++;
            case '4':
                sum++;
            case '3':
                sum++;
            case '2':
                sum++;
            case '1':
                sum++;
            case '0':
                break;
            default:
                return false;
        }

        pointer++;
    }

    *out = (int) sum;

    return true;
}

static bool parseHexadecimal(char *str, unsigned int *out)
{
    unsigned int sum     = 0;
    unsigned int prevSum = 0;
    char        *pointer = str;

    while (*pointer != '\0')
    {
        prevSum = sum;
        sum *= 0x10;
        if (sum < prevSum)
        {
            return false;
        }


        switch (*pointer)
        {
            case 'F':
            case 'f':
                sum++;
            case 'E':
            case 'e':
                sum++;
            case 'D':
            case 'd':
                sum++;
            case 'C':
            case 'c':
                sum++;
            case 'B':
            case 'b':
                sum++;
            case 'A':
            case 'a':
                sum++;
            case '9':
                sum++;
            case '8':
                sum++;
            case '7':
                sum++;
            case '6':
                sum++;
            case '5':
                sum++;
            case '4':
                sum++;
            case '3':
                sum++;
            case '2':
                sum++;
            case '1':
                sum++;
            case '0':
                break;
            default:
                return false;
        }

        pointer++;
    }

    if (sum > 0xFFFFFFFF)
    {
        return false;
    }

    *out = (int) sum;

    return true;
}

bool parseLiteral(char *literal, unsigned int *out)
{
    if (NULL == literal ||
        NULL == out)
    {
        return false;
    }

    if (strlen(literal) >= 3)
    {
        // Has a prefix
        if (0 == strncmp(literal, "0b", 2))
        {
            // Binary
            return parseBinary(&(literal[2]), out);

        }
        else if (0 == strncmp(literal, "0x", 2))
        {
            // Hexadecimal
            return parseHexadecimal(&(literal[2]), out);
        }
    }

    return parseDecimal(literal, out);
}

void freeFunctionContents(function_t *func)
{
    if (NULL == func)
    {
        return;
    }

    if (NULL != func->identifier)
    {
        free(func->identifier);
        func->identifier = NULL;
    }

    if (NULL != func->parameterTypes.first)
    {
        type_t *cur = func->parameterTypes.first;
        type_t *nxt = NULL;

        while (cur != NULL)
        {
            nxt = cur->next;
            free(cur);
            cur = nxt;
        }

        func->parameterTypes.first = NULL;
        func->parameterTypes.last  = NULL;
    }

    if (NULL != func->parameterNames.first)
    {
        freeStringLinkedListContents(&(func->parameterNames));
    }


    if (NULL != func->definition.codeBlock.firstItem)
    {
        freeVoidListContents(&(func->definition.codeBlock));
    }
}

void freeFunctionListContents(functionList_t *fl)
{
    function_t *cur;
    function_t *nxt;

    if (NULL == fl ||
        NULL == fl->first)
    {
        return;
    }

    cur = fl->first;

    while (NULL != cur)
    {
        nxt = cur->next;

        freeFunctionContents(cur);

        free(cur);

        cur = nxt;
    }
}

variable_t *findVariable(variableList_t *varList, char *ident)
{
    if (NULL == varList ||
        NULL == ident)
    {
        INTERNAL_ERROR;
        return NULL;
    }

    for (variable_t *v = varList->first; NULL != v; v = v->next)
    {
        if (0 == strcmp(v->identifier, ident))
        {
            return v;
        }
    }

    return NULL;
}

void freeVariableListContents(variableList_t *varList)
{
    variable_t *cur = NULL;
    variable_t *nxt = NULL;

    if (NULL == varList)
    {
        return;
    }

    cur = varList->first;

    while (cur != NULL)
    {
        nxt = cur->next;

        free(cur->identifier);
        free(cur);
        cur = nxt;
    }
}

voidContainer_t *addVoidContainer(voidList_t *vl)
{
    voidContainer_t *vc = NULL;

    if (NULL == vl)
    {
        return NULL;
    }

    vc = calloc(1, sizeof(*vc));

    if (NULL == vl->firstItem)
    {
        vl->firstItem = vc;
        vl->lastItem  = vc;

        return vc;
    }

    vl->lastItem->nextVoidContainer = vc;
    vl->lastItem                    = vc;

    return vc;
}

void freeVoidListContents(voidList_t *vl)
{
    voidContainer_t *cur = NULL;
    voidContainer_t *nxt = NULL;
    bool             freeData = false;

    if (NULL == vl)
    {
        return;
    }

    cur = vl->firstItem;

    while (cur != NULL)
    {
        nxt = cur->nextVoidContainer;

        if (NULL != cur->data)
        {
            freeData = true;
            switch (cur->type)
            {
                case codeBlock_e:
                {
                    INTERNAL_ERROR;
                    break;
                }
                case function_e:
                {
                    freeFunctionContents((function_t*) cur->data);
                    break;
                }
                case stackFrame_e:
                {
                    INTERNAL_ERROR;
                    break;
                }
                case expression_e:
                {
                    freeExpression((expression_t**) &cur->data);
                    freeData = false;
                    break;
                }
                case variable_e:
                {
                    INTERNAL_ERROR;
                    break;
                }
                case typedef_e:
                {
                    INTERNAL_ERROR;
                    break;
                }
                case type_e:
                {
                    // Nothing to free
                    break;
                }
                case if_e:
                {
                    if_t *i = (if_t*) cur->data;

                    if (NULL != i->condition)
                    {
                        freeExpression(&i->condition);
                    }
                    break;
                }
                default:
                {
                    INTERNAL_ERROR;
                    break;
                }
            }

            if (freeData)
            {
                free(cur->data);
            }
        }

        free(cur);
        cur = nxt;
    }

    vl->firstItem = NULL;
    vl->lastItem  = NULL;
}

void freeExpression(expression_t **exp)
{
    if (NULL == exp ||
        NULL == *exp)
    {
        return;
    }

    switch ((*exp)->expressionType)
    {
        case et_variable_e:
        {
            break;
        }
        case et_literal_e:
        {
            break;
        }
        case et_stringLiteral_e:
        {
            free((*exp)->contents.stringLiteral);
            break;
        }
        case et_unary_e:
        {
            freeExpression(&(*exp)->contents.unary.operand);
            break;
        }
        case et_binary_e:
        {
            freeExpression(&(*exp)->contents.binary.operand1);
            freeExpression(&(*exp)->contents.binary.operand2);
            break;
        }
        case et_trinary_e:
        {
            freeExpression(&(*exp)->contents.trinary.operand1);
            freeExpression(&(*exp)->contents.trinary.operand2);
            freeExpression(&(*exp)->contents.trinary.operand3);
            break;
        }
        case et_functionCall_e:
        {
            INTERNAL_ERROR;
            break;
        }
    }

    free(*exp);
    *exp = NULL;
}

void freeTypedefListContents(typedefs_t *t)
{
    typedef_t *cur;
    typedef_t *nxt;

    if (NULL == t)
    {
        return;
    }

    cur = t->first;

    while (cur != NULL)
    {
        nxt = cur->next;

        if (NULL != cur->identifier)
        {
            free(cur->identifier);
        }

        free(cur);

        cur = nxt;
    }
}

void freeParsedDataContents(parsedData_t *p)
{
    if (NULL == p)
    {
        return;
    }

    freeTypedefListContents(&p->typedefs);
    freeVariableListContents(&p->globalVariables);
    freeFunctionListContents(&p->functions);
}
