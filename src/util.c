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

static inline bool isCharForVar(char c)
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
            else if (false == isCharForVar(buf[i]))
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
            false == isCharForVar(buf[i]))
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
