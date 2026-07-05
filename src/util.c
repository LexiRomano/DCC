#include "dcc.h"

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
