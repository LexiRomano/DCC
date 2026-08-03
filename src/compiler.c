#include "dcc.h"

static FILE *inputFile  = NULL;
static int   lineNumber = 1;
static char *fileName   = NULL;

static int lineRewind = 1;
static int offsetRewind = 0;

static bool  error      = false;

static typedefs_t     typedefs        = {0};
static variableList_t globalVariables = {0};
static functionList_t functions       = {0};


char *g_keywords[] =
{
    "int", "char", "do", "while", "for", "if", "else", "const",
    "static", "return", "continue", "break", "typedef", "struct",
    "void", "NULL", "unsigned" "switch", "case",
    NULL
};

static int operatorPrecedence[] =
{
    1,  // op_parenthesis_e
    1,  // op_variable_e
    1,  // op_literal_e

    2,  // op_arrayIndexing_e
    2,  // op_functionCall_e
    2,  // op_memberAccess_e
    2,  // op_memberAccessPt_e
    2,  // op_postIncrement_e
    2,  // op_postDecrement_e

    3,  // op_preIncrement_e
    3,  // op_preDecrement_e
    3,  // op_reference_e
    3,  // op_dereference_e
    3,  // op_bitwiseNot_e
    3,  // op_logicalNot_e
    3,  // op_negative_e

    4,  // op_cast_e

    5,  // op_multiplication_e
    5,  // op_division_e
    5,  // op_modulus_e

    6,  // op_subtraction_e
    6,  // op_addition_e

    7,  // op_bitshiftLeft_e
    7,  // op_bitshiftRight_e

    8,  // op_lessThan_e
    8,  // op_greaterThan_e
    8,  // op_lessEqual_e
    8,  // op_greaterEqual_e

    9,  // op_equals_e
    9,  // op_notEquals_e

    10, // op_bitwiseAnd_e

    11, // op_bitwiseXor_e

    12, // op_bitwiseOr_e

    13, // op_logicalAnd_e

    14, // op_logicalOr_e

    15, // op_conditional_e

    16, // op_assignment_e
    16, // op_addAssignment_e
    16, // op_subtractAssignment_e
    16, // op_multiplyAssignment_e
    16, // op_divideAssignment_e
    16, // op_modulusAssignment_e
};

// Special cases are left empty
static char *operatorTokens[] =
{
    "",   // op_parenthesis_e
    "",   // op_variable_e
    "",   // op_literal_e
    "",   // op_arrayIndexing_e
    "",   // op_functionCall_e
    "",   // op_memberAccess_e
    "",   // op_memberAccessPt_e
    "++", // op_postIncrement_e
    "--", // op_postDecrement_e
    "++", // op_preIncrement_e
    "--", // op_preDecrement_e
    "&",  // op_reference_e
    "*",  // op_dereference_e
    "~",  // op_bitwiseNot_e
    "!",  // op_logicalNot_e
    "-",   // op_negative_e
    "",   // op_cast_e
    "*",   // op_multiplication_e
    "/",  // op_division_e
    "%",  // op_modulus_e
    "-",   // op_subtraction_e
    "+",  // op_addition_e
    "<<", // op_bitshiftLeft_e
    ">>", // op_bitshiftRight_e
    "<",  // op_lessThan_e
    ">",  // op_greaterThan_e
    "<=", // op_lessEqual_e
    ">=", // op_greaterEqual_e
    "==", // op_equals_e
    "!=", // op_notEquals_e
    "&",  // op_bitwiseAnd_e
    "^",  // op_bitwiseXor_e
    "|",  // op_bitwiseOr_e
    "&&", // op_logicalAnd_e
    "||", // op_logicalOr_e
    "",   // op_conditional_e
    "=",  // op_assignment_e
    "+=", // op_addAssignment_e
    "-=", // op_subtractAssignment_e
    "*=", // op_multiplyAssignment_e
    "/=", // op_divideAssignment_e
    "%=", // op_modulusAssignment_e
};

// Special cases as shown above are not important
static bool operatorAttatchLeft[] =
{
    true,  // op_parenthesis_e
    true,  // op_variable_e
    true,  // op_literal_e
    true,  // op_arrayIndexing_e
    true,  // op_functionCall_e
    true,  // op_memberAccess_e
    true,  // op_memberAccessPt_e
    true,  // op_postIncrement_e
    true,  // op_postDecrement_e
    false, // op_preIncrement_e
    false, // op_preDecrement_e
    false, // op_reference_e
    false, // op_dereference_e
    false, // op_bitwiseNot_e
    false, // op_logicalNot_e
    false, // op_negative_e
    true,  // op_cast_e
    true,  // op_multiplication_e
    true,  // op_division_e
    true,  // op_modulus_e
    true,  // op_subtraction_e
    true,  // op_addition_e
    true,  // op_bitshiftLeft_e
    true,  // op_bitshiftRight_e
    true,  // op_lessThan_e
    true,  // op_greaterThan_e
    true,  // op_lessEqual_e
    true,  // op_greaterEqual_e
    true,  // op_equals_e
    true,  // op_notEquals_e
    true,  // op_bitwiseAnd_e
    true,  // op_bitwiseXor_e
    true,  // op_bitwiseOr_e
    true,  // op_logicalAnd_e
    true,  // op_logicalOr_e
    true,  // op_conditional_e
    false, // op_assignment_e
    false, // op_addAssignment_e
    false, // op_subtractAssignment_e
    false, // op_multiplyAssignment_e
    false, // op_divideAssignment_e
    false, // op_modulusAssignment_e
};

// Special cases as shown above are not important
static int operatorOperandCount[] =
{
    0, // op_parenthesis_e
    0, // op_variable_e
    0, // op_literal_e
    0, // op_arrayIndexing_e
    0, // op_functionCall_e
    0, // op_memberAccess_e
    0, // op_memberAccessPt_e
    1, // op_postIncrement_e
    1, // op_postDecrement_e
    1, // op_preIncrement_e
    1, // op_preDecrement_e
    1, // op_reference_e
    1, // op_dereference_e
    1, // op_bitwiseNot_e
    1, // op_logicalNot_e
    1, // op_negative_e
    0, // op_cast_e
    2, // op_multiplication_e
    2, // op_division_e
    2, // op_modulus_e
    2, // op_subtraction_e
    2, // op_addition_e
    2, // op_bitshiftLeft_e
    2, // op_bitshiftRight_e
    2, // op_lessThan_e
    2, // op_greaterThan_e
    2, // op_lessEqual_e
    2, // op_greaterEqual_e
    2, // op_equals_e
    2, // op_notEquals_e
    2, // op_bitwiseAnd_e
    2, // op_bitwiseXor_e
    2, // op_bitwiseOr_e
    2, // op_logicalAnd_e
    2, // op_logicalOr_e
    0, // op_conditional_e
    2, // op_assignment_e
    2, // op_addAssignment_e
    2, // op_subtractAssignment_e
    2, // op_multiplyAssignment_e
    2, // op_divideAssignment_e
    2, // op_modulusAssignment_e
};

static char *getNextTokenCheckingForLineChange()
{
    char *nextToken = getNextTokenFromFile(inputFile, &lineNumber);

    if (NULL == nextToken)
    {
        return NULL;
    }

    /*
    if (0 == strcmp("#", nextToken))
    {
        // TODO
    }
    */

    return nextToken;
}

static void saveForRewindTokenParse()
{
    lineRewind   = lineNumber;
    offsetRewind = ftell(inputFile);
}

static void rewindTokenParse()
{
    lineNumber = lineRewind;
    fseek(inputFile, offsetRewind, SEEK_SET);
}

void drainToNextSemicolon()
{
    char *token        = NULL;
    int   bracketDepth = 0;
    int   braceDepth   = 0;
    int   parenDepth   = 0;

    while (true)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            return;
        }

        if (0 == bracketDepth &&
            0 == braceDepth   &&
            0 == parenDepth   &&
            0 == strcmp(";", token))
        {
            free(token);
            return;
        }

        if (0 == strcmp("[", token))
        {
            bracketDepth++;
        }
        else if (0 == strcmp("]", token))
        {
            bracketDepth--;
        }
        else if (0 == strcmp("{", token))
        {
            braceDepth++;
        }
        else if (0 == strcmp("}", token))
        {
            braceDepth--;
        }
        else if (0 == strcmp("(", token))
        {
            parenDepth++;
        }
        else if (0 == strcmp(")", token))
        {
            parenDepth--;
        }
    }
}

void drainToEndOfBlock(bool alreadyInBlock)
{
    char *token      = NULL;
    int   braceDepth = 0;

    if (alreadyInBlock)
    {
        braceDepth++;
    }

    while (true)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            return;
        }

        if (0 == strcmp("{", token))
        {
            braceDepth++;
        }
        else if (0 == strcmp("}", token))
        {
            braceDepth--;
            if (0 == braceDepth)
            {
                free(token);
                return;
            }
        }
    }
}

void drainToEndOfParenthasis(bool alreadyInParenthesis)
{
    char *token      = NULL;
    int   parenDepth = 0;

    if (alreadyInParenthesis)
    {
        parenDepth++;
    }

    while (true)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            return;
        }

        if (0 == strcmp("(", token))
        {
            parenDepth++;
        }
        else if (0 == strcmp(")", token))
        {
            parenDepth--;
            if (0 == parenDepth)
            {
                free(token);
                return;
            }
        }
    }
}

static void printErrorDuplicateKeyword(char *keyword)
{
    char *line = getCurrentLine(inputFile);

    ERROR_ARGS(fileName, lineNumber, line, "duplicate keyword \"%s\"", keyword);

    free(line);

    error = true;
}

#define printError(msg)                   \
do {                                       \
    char *line = getCurrentLine(inputFile); \
    ERROR(fileName, lineNumber, line, msg);  \
    free(line);                               \
    error = true;                              \
} while (false)

#define printErrorArgs(msg, ...)                           \
do {                                                        \
    char *line = getCurrentLine(inputFile);                  \
    ERROR_ARGS(fileName, lineNumber, line, msg, __VA_ARGS__); \
    free(line);                                                \
    error = true;                                               \
} while (false)

// Caller to free result
static type_t *findType(char *str)
{
    type_t *out = NULL;

    if (0 == strcmp(str, "int"))
    {
        out = calloc(1, sizeof(*out));

        out->isRaw = true;
        out->type.rawType = int_e;

        return out;
    }

    if (0 == strcmp(str, "char"))
    {
        out = calloc(1, sizeof(*out));

        out->isRaw = true;
        out->type.rawType = char_e;

        return out;
    }

    if (0 == strcmp(str, "void"))
    {
        out = calloc(1, sizeof(*out));

        out->isVoid = true;
        
        return out;
    }

    for (typedef_t *t = typedefs.first; NULL != t; t = t->next)
    {
        if (0 == strcmp(str, t->identifier))
        {
            out = calloc(1, sizeof(*out));
            out->isRaw = false;
            out->type.typeDefinition = t;

            return out;
        }
    }

    return NULL;
}

static bool isTypeRaw(type_t *t)
{
    if (NULL == t ||
        t->isVoid ||
        t->pointerDepth > 0)
    {
        return false;
    }

    if (t->isRaw ||
        t->type.typeDefinition->simplifiesToRawType)
    {
        return true;
    }

    return false;
}

static bool isTypePointer(type_t *t)
{
    if (NULL == t ||
        t->pointerDepth == 0)
    {
        return false;
    }

    return true;
}

static char *getTypeString(type_t *t)
{
    char buf[64] = {0};

    if (NULL == t)
    {
        return NULL;
    }

    if (t->isVoid)
    {
        snprintf(buf, sizeof(buf), "void");
    }
    else if (t->isRaw)
    {
        switch (t->type.rawType)
        {
            case int_e:
            {
                snprintf(buf, sizeof(buf), "int");
                break;
            }
            case uint_e:
            {
                snprintf(buf, sizeof(buf), "unsigned int");
                break;
            }
            case hint_e:
            {
                snprintf(buf, sizeof(buf), "short int");
                break;
            }
            case huint_e:
            {
                snprintf(buf, sizeof(buf), "short unsigned int");
                break;
            }
            case char_e:
            {
                snprintf(buf, sizeof(buf), "char");
                break;
            }
            case uchar_e:
            {
                snprintf(buf, sizeof(buf), "unsigned char");
                break;
            }
        }
    }
    else
    {
        snprintf(buf, sizeof(buf), "%s", t->type.typeDefinition->identifier);
    }

    if (t->pointerDepth > 0)
    {
        for (int i = 0; i < t->pointerDepth; i++)
        {
            snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "*");
        }
    }

    return strcpy(calloc(strlen(buf) + 1, sizeof(char)), buf);
}

static bool isReferencable(expression_t *exp)
{
    expression_t *childExp = NULL;

    if (NULL == exp)
    {
        return false;
    }

    childExp = exp;
    while (true)
    {
        switch (childExp->expressionType)
        {
            case et_variable_e:
            {
                return true;
            }
            case et_unary_e:
            {
                switch (childExp->contents.unary.operation)
                {
                    case op_parenthesis_e:
                    {
                        childExp = childExp->contents.unary.operand;
                        continue;
                    }
                    case op_dereference_e:
                    {
                        return true;
                    }
                    default:
                    {
                        return false;
                    }
                }
            }
            default:
            {
                return false;
            }
        }
    }
}

static bool areTypesSimilar(type_t *t1, type_t *t2)
{
    if (NULL == t1 ||
        NULL == t2)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (t1->isVoid       != t2->isVoid ||
        t1->pointerDepth != t2->pointerDepth)
    {
        return false;
    }

    if (t1->isVoid)
    {
        return true;
    }

    if ((t1->isRaw || t1->type.typeDefinition->simplifiesToRawType) &&
        (t2->isRaw || t2->type.typeDefinition->simplifiesToRawType))
    {
        return true;
    }

    return t1->type.typeDefinition == t2->type.typeDefinition;
}

static bool evaluateType(expression_t *exp)
{
    type_t *resType    = NULL;
    type_t *childType1 = NULL;
    type_t *childType2 = NULL;
    char   *typeStr1   = NULL;
    char   *typeStr2   = NULL;
    
    if (NULL == exp)
    {
        return true;
    }

    resType = &exp->resultingType;


    switch (exp->expressionType)
    {
        case et_variable_e:
        {
            memcpy(resType, &exp->contents.variable->type, sizeof(*resType));
            return true;
        }
        case et_literal_e:
        {
            resType->isVoid       = false;
            resType->isRaw        = true;
            resType->type.rawType = int_e;
            resType->pointerDepth = 0;
            return true;
        }
        case et_stringLiteral_e:
        {
            resType->isVoid       = false;
            resType->isRaw        = true;
            resType->type.rawType = char_e;
            resType->pointerDepth = 1;
            return true;
        }
        case et_unary_e:
        {
            if (false == evaluateType(exp->contents.unary.operand))
            {
                return false;
            }
            childType1 = &(exp->contents.unary.operand->resultingType);

            if (NULL == childType1)
            {
                INTERNAL_ERROR;
                return false;
            }

            if (childType1->isVoid &&
                childType1->pointerDepth == 0)
            {
                printError("cannot operate on void type");
                return false;
            }

            switch (exp->contents.unary.operation)
            {
                case op_postIncrement_e:
                case op_postDecrement_e:
                case op_preIncrement_e:
                case op_preDecrement_e:
                {
                    if (isTypeRaw    (childType1) ||
                        isTypePointer(childType1))
                    {
                        memcpy(resType, childType1, sizeof(*resType));
                        return true;
                    }
                    typeStr1 = getTypeString(childType1);
                    printErrorArgs("cannot apply \"%s\" operator to '%s'",
                                   operatorTokens[exp->contents.unary.operation], typeStr1);
                    free(typeStr1);
                    return false;
                }
                case op_bitwiseNot_e:
                case op_negative_e:
                {
                    if (isTypeRaw(childType1))
                    {
                        memcpy(resType, childType1, sizeof(*resType));
                        return true;
                    }
                    typeStr1 = getTypeString(childType1);
                    printErrorArgs("cannot apply \"%s\" operator to '%s'",
                                   operatorTokens[exp->contents.unary.operation], typeStr1);
                    free(typeStr1);
                    return false;
                }
                case op_reference_e:
                {
                    if (false == isReferencable(exp->contents.unary.operand))
                    {
                        typeStr1 = getTypeString(childType1);
                        printErrorArgs("cannot dereference '%s'", typeStr1);
                        free(typeStr1);
                        return false;
                    }
                    memcpy(resType, childType1, sizeof(*resType));
                    resType->pointerDepth++;
                    return true;
                }
                case op_dereference_e:
                {
                    if (childType1->pointerDepth == 0)
                    {
                        printError("only pointers can be dereferenced");
                        return false;
                    }
                    if (childType1->pointerDepth == 1 &&
                        childType1->isVoid)
                    {
                        printError("cannot dereference NULL pointers");
                        return false;
                    }
                    memcpy(resType, childType1, sizeof(*resType));
                    resType->pointerDepth--;
                    return true;
                }
                case op_parenthesis_e:
                {
                    memcpy(resType, childType1, sizeof(*resType));
                    return true;
                }
                default:
                {
                    INTERNAL_ERROR;
                    return false;
                }
            }
        }
        case et_binary_e:
        {
            if (false == evaluateType(exp->contents.binary.operand1) ||
                false == evaluateType(exp->contents.binary.operand2))
            {
                return false;
            }

            childType1 = &(exp->contents.binary.operand1->resultingType);
            childType2 = &(exp->contents.binary.operand2->resultingType);

            if (NULL == childType1 ||
                NULL == childType2)
            {
                INTERNAL_ERROR;
                return false;
            }

            if ((childType1->isVoid &&
                 childType1->pointerDepth == 0) ||
                (childType2->isVoid &&
                 childType2->pointerDepth == 0))
            {
                printError("cannot operate on void type");
                return false;
            }
            switch (exp->contents.binary.operation)
            {
                case op_addition_e:
                {
                    if (isTypeRaw(childType1) && isTypeRaw(childType2))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }
                    if (isTypePointer(childType1) && isTypeRaw(childType2))
                    {
                        memcpy(resType, childType1, sizeof(*resType));
                        return true;
                    }
                    if (isTypeRaw(childType1) && isTypePointer(childType2))
                    {
                        memcpy(resType, childType2, sizeof(*resType));
                        return true;
                    }
                    break;
                }
                case op_subtraction_e:
                {
                    if (isTypeRaw(childType1) && isTypeRaw(childType2))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }
                    if (isTypePointer(childType1) && isTypeRaw(childType2))
                    {
                        memcpy(resType, childType1, sizeof(*resType));
                        return true;
                    }
                    break;
                }
                case op_multiplication_e:
                case op_division_e:
                case op_bitshiftLeft_e:
                case op_bitshiftRight_e:
                case op_bitwiseAnd_e:
                case op_bitwiseOr_e:
                case op_bitwiseXor_e:
                {
                    if (isTypeRaw(childType1) && isTypeRaw(childType2))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }
                    break;
                }
                case op_equals_e:
                case op_notEquals_e:
                case op_lessThan_e:
                case op_greaterEqual_e:
                {
                    if ((isTypeRaw(childType1) || isTypePointer(childType1)) &&
                        (isTypeRaw(childType2) || isTypePointer(childType2)))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }
                    break;
                }
                case op_assignment_e:
                {
                    if (isTypeRaw(childType1) && isTypeRaw(childType2))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }
                    if (isTypePointer(childType1) && isTypePointer(childType2) &&
                        areTypesSimilar(childType1, childType2))
                    {
                        memcpy(resType, childType1, sizeof(*resType));
                        return true;
                    }

                    typeStr1 = getTypeString(childType1);
                    typeStr2 = getTypeString(childType2);
                    printErrorArgs("cannot assign '%s' to '%s'", typeStr1, typeStr2);
                    free(typeStr1);
                    free(typeStr2);
                    return false;
                }
                case op_addAssignment_e:
                case op_subtractAssignment_e:
                {
                    if (isTypeRaw(childType1) && isTypeRaw(childType2))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }

                    if (isTypeRaw(childType1) && isTypePointer(childType2))
                    {
                        memcpy(resType, childType2, sizeof(*resType));
                        return true;
                    }

                    typeStr1 = getTypeString(childType1);
                    typeStr2 = getTypeString(childType2);
                    printErrorArgs("cannot perform %s on '%s' with '%s'", 
                                   operatorTokens[exp->contents.binary.operation],
                                   typeStr2, typeStr1);
                    free(typeStr1);
                    free(typeStr2);
                    return false;
                }
                case op_multiplyAssignment_e:
                case op_divideAssignment_e:
                case op_modulusAssignment_e:
                {
                    if (isTypeRaw(childType1) && isTypeRaw(childType2))
                    {
                        resType->isVoid       = false;
                        resType->isRaw        = true;
                        resType->type.rawType = int_e;
                        resType->pointerDepth = 0;
                        return true;
                    }

                    typeStr1 = getTypeString(childType1);
                    typeStr2 = getTypeString(childType2);
                    printErrorArgs("cannot perform %s on '%s' with '%s'", 
                                   operatorTokens[exp->contents.binary.operation],
                                   typeStr2, typeStr1);
                    free(typeStr1);
                    free(typeStr2);
                    return false;
                }
                default:
                {
                    printf("%s", operatorTokens[exp->contents.binary.operation]);
                    INTERNAL_ERROR;
                    return false;
                }
            }

            typeStr1 = getTypeString(childType1);
            typeStr2 = getTypeString(childType2);
            printErrorArgs("invalid operands for %s (have '%s' and '%s')",
                            operatorTokens[exp->contents.binary.operation],
                            typeStr1, typeStr2);
            free(typeStr1);
            free(typeStr2);
            return false;
        }
        case et_trinary_e:
        {
            printError("trinary type evaluation not supported");
            return false;
        }
        case et_functionCall_e:
        {
            printError("function call type evaluation not supported");
            return true;
        }
        default:
        {
            INTERNAL_ERROR;
            return false;
        }
    }
}

static bool expressionRequiresOperand(expression_t *exp)
{
    if (NULL == exp)
    {
        return false;
    }

    switch (exp->expressionType)
    {
        case et_unary_e:
        {
            return NULL == exp->contents.unary.operand;
        }
        case et_binary_e:
        {
            return NULL == exp->contents.binary.operand1 ||
                   NULL == exp->contents.binary.operand2;
        }
        case et_trinary_e:
        {
            return NULL == exp->contents.trinary.operand1 ||
                   NULL == exp->contents.trinary.operand2 ||
                   NULL == exp->contents.trinary.operand3;
        }
        default:
        {
            return false;
        }
    }
}

static bool getOperation(expression_t *exp,
                         operation_e  *out)
{
    if (NULL == exp ||
        NULL == out)
    {
        return false;
    }

    switch (exp->expressionType)
    {
        case et_unary_e:
        {
            *out = exp->contents.unary.operation;
            break;
        }
        case et_binary_e:
        {
            *out = exp->contents.binary.operation;
            break;
        }
        case et_trinary_e:
        {
            *out = exp->contents.trinary.operation;
            break;
        }
        case et_variable_e:
        {
            *out = op_variable_e;
            break;
        }
        case et_literal_e:
        case et_stringLiteral_e:
        {
            *out = op_literal_e;
            break;
        }
        default:
        {
            return false;
        }
    }

    return true;
}

static bool getDesiredSwapPoint(expression_t   *in,
                                expression_t ***out)
{
    if (NULL == in ||
        NULL == out)
    {
        return false;
    }

    switch (in->expressionType)
    {
        case et_unary_e:
        {
            *out = &in->contents.unary.operand;
            break;
        }
        case et_binary_e:
        {
            if (false == operatorAttatchLeft[in->contents.binary.operation])
            {
                *out = &in->contents.binary.operand1;
            }
            else
            {
                *out = &in->contents.binary.operand2;
            }
            break;
        }
        case et_trinary_e:
        {
            *out = &in->contents.trinary.operand3;
            break;
        }
        default:
        {
            return false;
        }
    }

    return true;
}

static bool getEmptyExpression(expression_t   *in,
                               expression_t ***out)
{
    if (NULL == in ||
        NULL == out)
    {
        return false;
    }

    switch (in->expressionType)
    {
        case et_unary_e:
        {
            if (NULL == in->contents.unary.operand)
            {
                *out = &in->contents.unary.operand;
                return true;
            }
            break;
        }
        case et_binary_e:
        {
            if (false == operatorAttatchLeft[in->contents.binary.operation])
            {
                if (NULL == in->contents.binary.operand2)
                {
                    *out = &in->contents.binary.operand2;
                    return true;
                }
                if (NULL == in->contents.binary.operand1)
                {
                    *out = &in->contents.binary.operand1;
                    return true;
                }
            }
            else
            {
                if (NULL == in->contents.binary.operand1)
                {
                    *out = &in->contents.binary.operand1;
                    return true;
                }
                if (NULL == in->contents.binary.operand2)
                {
                    *out = &in->contents.binary.operand2;
                    return true;
                }
            }
            break;
        }
        case et_trinary_e:
        {
            if (NULL == in->contents.trinary.operand1)
            {
                *out = &in->contents.trinary.operand1;
            }
            if (NULL == in->contents.trinary.operand2)
            {
                *out = &in->contents.trinary.operand2;
            }
            if (NULL == in->contents.trinary.operand3)
            {
                *out = &in->contents.trinary.operand3;
            }
            break;
        }
        default:
        {
            return false;
        }
    }

    return false;
}

static bool bubbleUpExpression(expression_t **root,
                               expression_t  *current,
                               expression_t  *new)
{
    operation_e    eOp      = 0;
    operation_e    newOp    = 0;
    expression_t **relocate = NULL;
    expression_t **insert   = NULL;

    if (NULL == root    ||
        NULL == *root   ||
        NULL == current ||
        NULL == new)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (false == getOperation(new, &newOp))
    {
        INTERNAL_ERROR;
        return false;
    }

    for (expression_t *e = current; NULL != e; e = e->parent)
    {
        if (false == getOperation(e, &eOp))
        {
            INTERNAL_ERROR;
            return false;
        }

        if (false == operatorAttatchLeft[eOp] &&
            2     == operatorOperandCount[eOp])
        {
            if (operatorPrecedence[eOp] < operatorPrecedence[newOp])
            {
                continue;
            }
        }
        else if (operatorPrecedence[eOp] <= operatorPrecedence[newOp])
        {
            continue;
        }

        if (false == getDesiredSwapPoint(e, &relocate) ||
            false == getEmptyExpression(new, &insert))
        {
            INTERNAL_ERROR;
            return false;
        }

        *insert = *relocate;
        if (NULL != (*relocate))
        {
            (*relocate)->parent = new;
        }
        *relocate = new;
        new->parent = e;
        return true;
    }

    if (false == getEmptyExpression(new, &insert))
    {
        INTERNAL_ERROR;
        return false;
    }

    *insert = *root;
    (*root)->parent = new;
    *root = new;

    return true;
}

static bool parseExpression(stackFrame_t  *stackFrame,
                            expression_t **root);
static bool parseExpressionInternal(stackFrame_t  *stackFrame,
                                    expression_t **root,
                                    expression_t  *prevExp)
{
    char          *token   = NULL;
    variable_t    *tmpVar  = NULL;
    expression_t  *newExp  = NULL;
    expression_t  *newExp2 = NULL;
    expression_t **insert  = NULL;
    unsigned int   literal = 0;

    if (NULL == root)
    {
        INTERNAL_ERROR;
        return false;
    }

    saveForRewindTokenParse();

    token = getNextTokenCheckingForLineChange();

    if (NULL == token)
    {
        printError("expected expression");
        return false;
    }

    if (0 == strcmp(";", token) ||
        0 == strcmp(")", token))
    {
        rewindTokenParse();
        free(token);
        return true;
    }

    if (0 == strcmp("(", token))
    {
        free(token);
        token = NULL;

        if (NULL != prevExp &&
            false == getEmptyExpression(prevExp, &insert))
        {
            // Never got inserted into the tree, need to free here
            // This would be dealing with function pointers... let's not x_X
            drainToEndOfParenthasis(true);
            printError("function pointers not implemented");
            return false;
        }

        if (false == parseExpression(stackFrame, &newExp2))
        {
            return false;
        }

        if (NULL == newExp2)
        {
            printError("expected expression before \")\"");
            drainToEndOfParenthasis(true);
            return false;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token ||
            0    != strcmp(")", token))
        {
            printErrorArgs("expected \")\", got \"%s\"", token);
            freeExpression(&newExp2);
            free(token);
            return false;
        }

        free(token);

        newExp                           = calloc(1, sizeof(*newExp));
        newExp->expressionType           = et_unary_e;
        newExp->contents.unary.operation = op_parenthesis_e;
        newExp->contents.unary.operand   = newExp2;

        if (NULL == insert)
        {
            // First statement
            *root = newExp;
        }
        else
        {
            *insert        = newExp;
            newExp->parent = prevExp;
        }

        goto checkForEnd;

    }

    // Special handling required for certain operators before the generic loop

    for (operation_e op = 0; op_count_e != op; op++)
    {
        if (0 == strcmp(operatorTokens[op], token))
        {
            if (2 == operatorOperandCount[op])
            {
                // <complete statement> + <complete statement>
                if (NULL == prevExp ||
                    true == expressionRequiresOperand(prevExp))
                {
                    printErrorArgs("\"%s\" requires preceding statement", token);
                    free(token);
                    return false;
                }

                newExp                            = calloc(1, sizeof(*newExp));
                newExp->expressionType            = et_binary_e;
                newExp->contents.binary.operation = op;

                if (NULL == *root)
                {
                    *root = newExp;
                }
                else if (false == bubbleUpExpression(root, prevExp, newExp))
                {
                    // Never got inserted into the tree, need to free here
                    freeExpression(&newExp);
                    free(token);
                    return false;
                }

                if (false == parseExpressionInternal(stackFrame, root, newExp))
                {
                    free(token);
                    return false;
                }

                if (expressionRequiresOperand(newExp))
                {
                    printErrorArgs("expected statement after \"%s\"", token);
                    free(token);
                    return false;
                }

                free(token);
                return true;
            }

            if (operatorAttatchLeft[op] &&
                1 == operatorOperandCount[op])
            {
                // <complete statement> ++ [...]

                if (NULL == *root   ||
                    NULL == prevExp ||
                    true == expressionRequiresOperand(prevExp))
                {
                    if (op == op_postIncrement_e ||
                        op == op_postDecrement_e)
                    {
                        // Treat as pre-decrement, keep going
                        continue;
                    }

                    printErrorArgs("\"%s\" requires preceding statement", token);
                    free(token);
                    return false;
                }

                free(token);
                token = NULL;

                newExp                           = calloc(1, sizeof(*newExp));
                newExp->expressionType           = et_unary_e;
                newExp->contents.unary.operation = op;

                if (false == bubbleUpExpression(root, prevExp, newExp))
                {
                    // Never got inserted into the tree, need to free here
                    freeExpression(&newExp);
                    return false;
                }

                goto checkForEnd;
            }

            if (false == operatorAttatchLeft[op] &&
                1     == operatorOperandCount[op])
            {
                // [...] ++<complete statement>

                if (NULL  != prevExp &&
                    false == expressionRequiresOperand(prevExp))
                {
                    if (op == op_dereference_e ||
                        op == op_negative_e)
                    {
                        // Should instead be multiplication or subtraction
                        continue;
                    }
                    printErrorArgs("\"%s\" cannot precede a complete statement", token);
                    free(token);
                    return false;
                }

                newExp                           = calloc(1, sizeof(*newExp));
                newExp->expressionType           = et_unary_e;
                newExp->contents.unary.operation = op;

                if (NULL == *root)
                {
                    *root = newExp;
                }
                else if (false == bubbleUpExpression(root, prevExp, newExp))
                {
                    // Never got inserted into the tree, need to free here
                    freeExpression(&newExp);
                    free(token);
                    return false;
                }

                if (false == parseExpressionInternal(stackFrame, root, newExp))
                {
                    free(token);
                    return false;
                }

                if (expressionRequiresOperand(newExp))
                {
                    printErrorArgs("expected statement after \"%s\"", token);
                    free(token);
                    return false;
                }

                free(token);
                return true;
            }
        }
    }

    if (NULL != *root &&
        false == expressionRequiresOperand(prevExp))
    {
        printErrorArgs("expected operator or end of expression instead of %s", token);
        free(token);
        return false;
    }

    if (isIdentifier(token))
    {
        for (stackFrame_t *sf = stackFrame; NULL != sf; sf = sf->prevAccessableStackFrame)
        {
            if (NULL != (tmpVar = findVariable(&sf->variables, token)))
            {
                foundVariable:
                free(token);
                token = NULL;

                newExp                    = calloc(1, sizeof(*newExp));
                newExp->parent            = prevExp;
                newExp->expressionType    = et_variable_e;
                newExp->contents.variable = tmpVar;

                if (NULL == *root)
                {
                    *root = newExp;
                }
                else if (false == getEmptyExpression(prevExp, &insert))
                {
                    // Should have already checked for this
                    freeExpression(&newExp);
                    INTERNAL_ERROR;
                    return false;
                }
                else
                {
                    *insert = newExp;
                }

                goto checkForEnd;
            }
        }

        if (NULL != (tmpVar = findVariable(&globalVariables, token)))
        {
            goto foundVariable;
        }

        // TODO function calls

        printErrorArgs("undefined identifier \"%s\"", token);

        free(token);

        return false;
    }

    if (parseLiteral(token, &literal))
    {
        free(token);
        token = NULL;

        newExp                   = calloc(1, sizeof(*newExp));
        newExp->parent           = prevExp;
        newExp->expressionType   = et_literal_e;
        newExp->contents.literal = literal;

        if (NULL == *root)
        {
            *root = newExp;
        }
        else if (false == getEmptyExpression(prevExp, &insert))
        {
            // Should have already checked for this
            freeExpression(&newExp);
            INTERNAL_ERROR;
            return false;
        }
        else
        {
            *insert = newExp;
        }

        goto checkForEnd;
    }

    if (stringWrappedWith(token, '"'))
    {
        // Take off last quote
        token[strlen(token) - 1] = '\0';

        // Take off the first quote and shift down
        for (char *p = token; '\0' != *p; p++)
        {
            *p = p[1];
        }

        newExp                         = calloc(1, sizeof(*newExp));
        newExp->parent                 = prevExp;
        newExp->expressionType         = et_stringLiteral_e;
        newExp->contents.stringLiteral = token;
        token                          = NULL;

        if (NULL == *root)
        {
            *root = newExp;
        }
        else if (false == getEmptyExpression(prevExp, &insert))
        {
            // Should have already checked for this
            freeExpression(&newExp);
            INTERNAL_ERROR;
            return false;
        }
        else
        {
            *insert = newExp;
        }

        goto checkForEnd;
    }

    if (stringWrappedWith(token, '\''))
    {
        // Take off last quote
        token[strlen(token) - 1] = '\0';

        // Take off the first quote and shift down
        for (char *p = token; '\0' != *p; p++)
        {
            *p = p[1];
        }

        if (1 == strlen(token))
        {
            literal = token[0];
        }
        else if (2 == strlen(token) && '\\' == token[0])
        {
            //escape sequence, not yet implemented
            INTERNAL_ERROR;
            free(token);
            return false;
        }
        else
        {
            printErrorArgs("invalid char literal '%s'", token);
            free(token);
            return false;
        }

        newExp                   = calloc(1, sizeof(*newExp));
        newExp->parent           = prevExp;
        newExp->expressionType   = et_literal_e;
        newExp->contents.literal = literal;

        if (NULL == *root)
        {
            *root = newExp;
        }
        else if (false == getEmptyExpression(prevExp, &insert))
        {
            // Should have already checked for this
            freeExpression(&newExp);
            INTERNAL_ERROR;
            return false;
        }
        else
        {
            *insert = newExp;
        }

        goto checkForEnd;
    }

    printErrorArgs("something's wrong with \"%s\"", token);

    free(token);

    return false;

    checkForEnd:
    token = peekNextTokenFromFile(inputFile);

    if (0 == strcmp(";", token) ||
        0 == strcmp(")", token))
    {
        free(token);
        return true;
    }

    free(token);

    return parseExpressionInternal(stackFrame, root, newExp);
}

static bool parseExpression(stackFrame_t  *stackFrame,
                            expression_t **root)
{
    if (NULL ==  root ||
        NULL != *root)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (false == parseExpressionInternal(stackFrame, root, NULL) ||
        false == evaluateType(*root))
    {
        freeExpression(root);
        return false;
    }

    return true;
}

// the ; or = has already been parsed
static bool createVariable(type_t         *varType,
                           char           *identifier,
                           variableList_t *targetVarList,
                           stackFrame_t   *stackFrame,
                           bool            parseValue)
{
    variable_t *newVar   = NULL;
    char       *token    = NULL;
    char       *typeStr1 = NULL;
    char       *typeStr2 = NULL;

    if (NULL == varType    ||
        NULL == identifier ||
        NULL == targetVarList)
    {
        return false;
    }

    newVar = calloc(1, sizeof(*newVar));

    if (parseValue)
    {
        if (false == parseExpression(NULL, &newVar->initializer))
        {
            free(newVar);
            drainToNextSemicolon();
            return false;
        }

        if (NULL == newVar->initializer)
        {
            printError("expected expression");
            return false;
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token ||
            0    != strcmp(";", token))
        {
            printErrorArgs("expected \";\", got \"%s\"", token);
            free(token);
            return false;
        }
        free(token);
        token = NULL;

        if (false == areTypesSimilar(&newVar->initializer->resultingType, varType))
        {
            typeStr1 = getTypeString(&newVar->initializer->resultingType);
            typeStr2 = getTypeString(varType);
            printErrorArgs("cannot initialize to '%s' (expected '%s')",
                           typeStr1, typeStr2);
            free(typeStr1);
            free(typeStr2);
            freeExpression(&newVar->initializer);
            free(newVar);
            return false;
        }
    }


    newVar->identifier = strcpy(calloc(strlen(identifier) + 1, sizeof(char)), identifier);
    memcpy(&(newVar->type), varType, sizeof(*varType));

    if (NULL == targetVarList->first)
    {
        targetVarList->first = newVar;
        targetVarList->last  = newVar;
        return true;
    }

    targetVarList->last->next = newVar;
    targetVarList->last       = newVar;

    return true;
}

static bool applyVarDescriptors(bool isShort, bool isUnsigned, type_t *type)
{
    if (NULL == type)
    {
        INTERNAL_ERROR;
        return false;
    }

    if (isShort &&
        isUnsigned)
    {
        if (false == type->isRaw ||
            int_e != type->type.rawType)
        {
            printError("cannot apply short and unsigned to this type");
            return false;
        }

        type->type.rawType = huint_e;
        return true;
    }

    if (isShort)
    {
        if (false == type->isRaw ||
            int_e != type->type.rawType)
        {
            printError("cannot apply short to this type");
            return false;
        }

        type->type.rawType = hint_e;

        return true;
    }

    if (isUnsigned)
    {
        if (false == type->isRaw ||
            (int_e  != type->type.rawType &&
             char_e != type->type.rawType))
        {
            printError("cannot apply unsigned to this type");
            return false;
        }

        if (int_e == type->type.rawType)
        {
            type->type.rawType = uint_e;
            return true;
        }

        type->type.rawType = uchar_e;
    }

    return true;
}

static bool getTypeAndIdent(type_t **typeOut,
                            char   **identOut)
{
    char *token = NULL;

    bool isUnsigned = false;
    bool isShort    = false;

    type_t *tmpType   = NULL;
    type_t *foundType = NULL;

    bool rc = true;

    if (NULL ==  typeOut  ||
        NULL != *typeOut  ||
        NULL ==  identOut ||
        NULL != *identOut)
    {
        INTERNAL_ERROR;
    }

    while (true)
    {
        if (NULL != token)
        {
            free(token);
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            printError("incomplete statement");
            return false;
        }

        if (0 == strcmp("unsigned", token))
        {
            if (isUnsigned)
            {
                printErrorDuplicateKeyword("unsigned");
                rc = false;
                continue;
            }
            isUnsigned = true;
            continue;
        }
        if (0 == strcmp("short", token))
        {
            if (isShort)
            {
                printErrorDuplicateKeyword("short");
                rc = false;
                continue;
            }
            isShort = true;
            continue;
        }
        if (NULL != (tmpType = findType(token)))
        {
            // Found the base variable type
            if (NULL != foundType)
            {
                printError("multiple types specified");
                free(tmpType);
                rc = false;
                continue;
            }

            foundType = tmpType;
            tmpType   = NULL;
            
            rc = rc && applyVarDescriptors(isShort, isUnsigned, foundType);

            continue;
        }
        if (NULL != foundType &&
            0    == strcmp("*", token))
        {
            foundType->pointerDepth++;
            continue;
        }
        if (isIdentifier(token))
        {
            if (NULL == foundType)
            {
                printError("expected type before identifier");
                return false;
            }
            if (rc)
            {
                *identOut = token;
                *typeOut  = foundType;

                return true;
            }

            free(token);

            if (NULL != foundType)
            {
                free(foundType);
            }
            
            return false;
        }

        if (isKeyword(token))
        {
            printErrorArgs("unexpected keyword \"%s\"", token);
            return false;
        }

        printError("idk what you did, but this is wrong");
    }
}

static void doTypedef()
{
    char      *token      = NULL;
    type_t    *foundType  = NULL;
    char      *foundIdent = NULL;
    typedef_t *newTypedef = NULL;

    saveForRewindTokenParse();

    token = getNextTokenCheckingForLineChange();

    if (NULL == token)
    {
        printError("incomplete typedef");
        return;
    }

    if (0 == strcmp("struct", token))
    {
        // is a struct
        printError("structs not implemented");
        free(token);
        return;
    }
    if (0 == strcmp("enum", token))
    {
        // is an enum
        printError("enums not implemented");
        free(token);
        drainToNextSemicolon();
        return;
    }

    free(token);
    token = NULL;

    rewindTokenParse();

    if (false == getTypeAndIdent(&foundType, &foundIdent))
    {
        drainToNextSemicolon();
        return;
    }

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        0    != strcmp(";", token))
    {
        printError("Expected \";\"");
        if (NULL != token)
        {
            free(token);
        }
        return;
    }

    newTypedef = calloc(1, sizeof(*newTypedef));

    newTypedef->identifier = foundIdent;
    newTypedef->typedefType = typeExtension_e;
    newTypedef->simplifiesToRawType = isTypeRaw(foundType);
    memcpy(&(newTypedef->content.typeExtension), foundType, sizeof(*foundType));

    if (NULL == typedefs.first)
    {
        typedefs.first = newTypedef;
        typedefs.last  = newTypedef;
        return;
    }

    typedefs.last->next = newTypedef;
    typedefs.last       = newTypedef;
}

// Forward declaration
static bool processStackFrame(stackFrame_t *stackFrame);

// The "if" has already been parsed
static bool parseIfStatement(stackFrame_t *stackFrame,
                             if_t        **out)
{
    char         *token         = NULL;
    if_t         *newIf         = NULL;
    expression_t *newExpression = NULL;

    if (NULL == stackFrame ||
        NULL == out)
    {
        INTERNAL_ERROR;
        return false;
    }

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        0    != strcmp("(", token))
    {
        printErrorArgs("expected \"(\" instead of \"%s\"", token);
        if (NULL != token)
        {
            free(token);
        }
        return false;
    }

    free(token);
    token = NULL;

    if (false == parseExpression(stackFrame, &newExpression))
    {
        drainToEndOfParenthasis(true);
        return false;
    }

    if (NULL == newExpression)
    {
        printError("expected statement");
        return false;
    }

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        0    != strcmp(")", token))
    {
        printError("expected \")\"");
        freeExpression(&newExpression);
        newExpression = NULL;
        if (NULL != token)
        {
            free(token);
        }
        return false;
    }

    free(token);
    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        0    != strcmp("{", token))
    {
        printError("expected \"{\". Don't be gross >:[");
        freeExpression(&newExpression);
        newExpression = NULL;
        if (NULL != token)
        {
            free(token);
        }
        drainToNextSemicolon();
        return false;
    }

    newIf = calloc(1, sizeof(*newIf));
    newIf->condition = newExpression;
    newExpression = NULL;
    newIf->consequence.prevAccessableStackFrame = stackFrame;

    if (processStackFrame(&newIf->consequence))
    {
        *out = newIf;
        return true;
    }

    freeExpression(&newIf->condition);
    free(newIf);
    return false;
}

// The opening "{" has already been parsed
static bool processStackFrame(stackFrame_t *stackFrame)
{
    char *token = NULL;
    char *ident = NULL;
    bool  rc    = true;

    stackFrame_t    *tmpStackFrame    = NULL;
    variable_t      *tmpVar           = NULL;
    type_t          *tmpType          = NULL;
    voidContainer_t *tmpVoidContainer = NULL;
    expression_t    *newExpression    = NULL;
    if_t            *newIf            = NULL;

    if (NULL == stackFrame)
    {
        INTERNAL_ERROR;
        return false;
    }

    while (true)
    {
        saveForRewindTokenParse();

        if (NULL != token)
        {
            free(token);
        }

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            printError("expected \"}\"");
            return false;
        }

        if (0 == strcmp(";", token))
        {
            continue;
        }

        if (0 == strcmp("}", token))
        {
            free(token);
            token = NULL;

            if (rc)
            {
                return true;
            }
            break;
        }

        if (0 == strcmp("if", token))
        {
            if (parseIfStatement(stackFrame, &newIf))
            {
                tmpVoidContainer = addVoidContainer(&stackFrame->codeBlock);

                if (NULL == tmpVoidContainer)
                {
                    INTERNAL_ERROR;
                    continue;
                }

                tmpVoidContainer->type = if_e;
                tmpVoidContainer->data = newIf;
                newIf = NULL;
            }

            continue;
        }
        if (0 == strcmp("else", token))
        {
            tmpVoidContainer = stackFrame->codeBlock.lastItem;

            if (if_e != tmpVoidContainer->type ||
                NULL == ((if_t*)tmpVoidContainer->data)->condition)
            {
                printError("expected preceding \"if\"");
                continue;
            }

            free(token);
            token = getNextTokenCheckingForLineChange();

            if (NULL == token ||
                (0 != strcmp("if", token) &&
                 0 != strcmp("{",  token)))
            {
                printErrorArgs("expected \"if\" or \"{\", got \"%s\"", token);
                if (0 == strcmp("(", token))
                {
                    drainToEndOfParenthasis(true);
                }
                continue;
            }

            if (0 == strcmp("if", token))
            {
                if (parseIfStatement(stackFrame, &newIf))
                {
                    newIf->parent = tmpVoidContainer->data;

                    tmpVoidContainer = addVoidContainer(&stackFrame->codeBlock);

                    tmpVoidContainer->type = if_e;
                    tmpVoidContainer->data = newIf;
                    newIf = NULL;
                }

                continue;
            }


            newIf = calloc(1, sizeof(*newIf));
            newIf->consequence.prevAccessableStackFrame = stackFrame;

            if (false == processStackFrame(&newIf->consequence))
            {
                free(newIf);
                continue;
            }

            newIf->parent = tmpVoidContainer->data;

            tmpVoidContainer = addVoidContainer(&stackFrame->codeBlock);

            tmpVoidContainer->type = if_e;
            tmpVoidContainer->data = newIf;
            newIf = NULL;

            continue;
        }
        if (0 == strcmp("for", token))
        {
            //TODO
            printError("for statements not implemented");
            break;
        }
        if (0 == strcmp("do", token))
        {
            //TODO
            printError("do statements not implemented");
            break;
        }
        if (0 == strcmp("while", token))
        {
            //TODO
            printError("while statements not implemented");
            break;
        }
        if (0 == strcmp("switch", token))
        {
            //TODO
            printError("switch statements not implemented");
            break;
        }

        if (0 == strcmp("{", token))
        {
            free(token);
            token = NULL;
            tmpStackFrame = calloc(1, sizeof(*stackFrame));
            tmpStackFrame->prevAccessableStackFrame = stackFrame;
            if (processStackFrame(tmpStackFrame))
            {
                tmpVoidContainer = addVoidContainer(&stackFrame->codeBlock);

                if (NULL == tmpVoidContainer)
                {
                    INTERNAL_ERROR;
                    continue;
                }

                tmpVoidContainer->data = tmpStackFrame;
                tmpStackFrame = NULL;
                tmpVoidContainer->type = stackFrame_e;
            }
            else
            {
                free(tmpStackFrame);
                tmpStackFrame = NULL;
                rc = false;
            }

            continue;
        }

        if (0 == strcmp("unsigned", token) ||
            0 == strcmp("short",    token) ||
            NULL != (tmpType = findType(token)))
        {
            // Declaring/defining a variable
            free(token);
            token = NULL;


            rewindTokenParse();
            if (NULL != tmpType)
            {
                free(tmpType);
                tmpType = NULL;
            }

            if (false == getTypeAndIdent(&tmpType, &ident))
            {
                drainToNextSemicolon();
                rc = false;
                continue;
            }

            if (NULL != (tmpVar = findVariable(&(stackFrame->variables), ident)))
            {
                tmpVar = NULL;
                printErrorArgs("redefinition of variable \"%s\"", ident);
                free(tmpType);
                tmpType = NULL;
                free(ident);
                ident = NULL;
                drainToNextSemicolon();
                rc = false;
                continue;
            }

            token = getNextTokenCheckingForLineChange();

            if (NULL == token ||
                (0 != strcmp(";", token) &&
                 0 != strcmp("=", token)))
            {
                printErrorArgs("expected \";\" or\"=\", got \"%s\"", token);
                free(tmpType);
                tmpType = NULL;
                free(ident);
                ident = NULL;
                drainToNextSemicolon();
                rc = false;
                continue;
            }

            if (false == createVariable(tmpType,
                                        ident,
                                        &stackFrame->variables,
                                        stackFrame,
                                        0 == strcmp("=", token)))
            {
                rc = false;
            }
            free(tmpType);
            tmpType = NULL;
            free(ident);
            ident = NULL;
            continue;
        }

        rewindTokenParse();

        if (false == parseExpression(stackFrame, &newExpression))
        {
            drainToNextSemicolon();
            continue;
        }

        if (NULL == newExpression)
        {
            INTERNAL_ERROR;
            continue;
        }

        tmpVoidContainer = addVoidContainer(&stackFrame->codeBlock);

        if (NULL == tmpVoidContainer)
        {
            INTERNAL_ERROR;
            continue;
        }

        tmpVoidContainer->type = expression_e;
        tmpVoidContainer->data = newExpression;
        newExpression = NULL;
    }

    // Error path
    drainToEndOfBlock(true);
    freeVoidListContents(&(stackFrame->codeBlock));
    freeVariableList(&(stackFrame->variables));
    error = true;

    return false;
}

// The opening "(" has already been parsed
static void processFunction(type_t *returnType,
                            char   *identifier)
{
    function_t newFunction   = {0};
    char      *token         = NULL;
    bool       findArguments = false;
    type_t    *foundType     = NULL;
    char      *foundIdent    = NULL;
    strll_t   *tmpStrll      = NULL;

    if (NULL == returnType ||
        NULL == identifier)
    {
        INTERNAL_ERROR;
        return;
    }

    saveForRewindTokenParse();

    token = getNextTokenCheckingForLineChange();

    if (NULL == token)
    {
        printError("incomplete function declaration");
        return;
    }

    findArguments = true;
    if (0 == strcmp(")", token))
    {
        findArguments = false;
    }
    else if (0 == strcmp(",", token))
    {
        printError("expected a type specifier");
        return;
    }
    else
    {
        rewindTokenParse();
    }

    // Getting the arguments
    while (findArguments)
    {
        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        if (false == getTypeAndIdent(&foundType, &foundIdent))
        {
            freeFunctionContents(&newFunction);
            return;
        }

        tmpStrll = addStringLinkedList(&(newFunction.parameterNames));

        if (NULL == tmpStrll)
        {
            INTERNAL_ERROR;
            freeFunctionContents(&newFunction);
            return;
        }

        tmpStrll->str = foundIdent;
        foundIdent    = NULL;

        if (NULL == newFunction.parameterTypes.first)
        {
            newFunction.parameterTypes.first = foundType;
            newFunction.parameterTypes.last  = foundType;
        }
        else
        {
            newFunction.parameterTypes.last->next = foundType;
            newFunction.parameterTypes.last       = foundType;
        }

        foundType = NULL;
        

        token = getNextTokenCheckingForLineChange();

        if (NULL == token)
        {
            printError("incomplete function declaration");
            freeFunctionContents(&newFunction);
            return;
        }

        if (0 == strcmp(")", token))
        {
            break;
        }
        if (0 == strcmp(",", token))
        {
            continue;
        }

        printError("expected \",\" or \")\"");
        freeFunctionContents(&newFunction);
        while (true)
        {
            if (NULL != token)
            {
                free(token);
            }
            token = getNextTokenCheckingForLineChange();
            if (NULL == token)
            {
                return;
            }
            if (0 == strcmp(";", token))
            {
                free(token);
                return;
            }
            if (0 == strcmp("{", token))
            {
                free(token);
                drainToEndOfBlock(true);
                return;
            }
        }
    }

    if (NULL != token)
    {
        free(token);
    }

    token = getNextTokenCheckingForLineChange();

    if (NULL == token ||
        (0 != strcmp(";", token) &&
         0 != strcmp("{", token)))
    {
        printError("expected \";\" or \"{\"");
        return;
    }

    if (0 == strcmp("{", token))
    {
        processStackFrame(&(newFunction.definition));
        newFunction.isDefined = true;
    }

    free(token);


    memcpy(&newFunction.returnType, returnType, sizeof(*returnType));
    newFunction.identifier = strcpy(calloc(strlen(identifier) + 1, sizeof(char)), identifier);

    if (NULL == functions.first)
    {
        functions.first = memcpy(calloc(1, sizeof(newFunction)), &newFunction, sizeof(newFunction));
        functions.last  = functions.first;
    }
    else
    {
        functions.last->next = memcpy(calloc(1, sizeof(newFunction)), &newFunction, sizeof(newFunction));
        functions.last       = functions.last->next;
    }
}

static bool intake()
{
    char *token = NULL;

    bool  isUnsigned = false;
    bool  isShort    = false;
    char *ident      = NULL;
    //int   pointerDepth = 0;

    bool  noMoreTokens = false;
    type_t *foundType = NULL;
    type_t *tmpType   = NULL;

    while (!noMoreTokens)
    {
        while (true)
        {
            if (NULL != token)
            {
                free(token);
            }
            token = getNextTokenCheckingForLineChange();

            if (NULL == token)
            {
                noMoreTokens = true;
                break;
            }

            if (NULL != ident)
            {
                if (0 == strcmp("=", token))
                {
                    // variable declaration + assignment
                    createVariable(foundType, ident, &globalVariables, NULL, true);
                    break;
                }
                if (0 == strcmp(";", token))
                {
                    // variable declaration

                    createVariable(foundType, ident, &globalVariables, NULL, false);

                    break;
                }
                if (0 == strcmp("(", token))
                {
                    // function declaration/definition
                    processFunction(foundType, ident);
                    break;
                }

                printError("expected \"=\", \";\", or \"(\"");
                drainToNextSemicolon();

                break;
            }

            if (0 == strcmp("typedef", token))
            {
                doTypedef();
                break;
            }
            if (0 == strcmp("unsigned", token))
            {
                if (isUnsigned)
                {
                    printErrorDuplicateKeyword("unsigned");
                    continue;
                }
                isUnsigned = true;
                continue;
            }
            if (0 == strcmp("short", token))
            {
                if (isShort)
                {
                    printErrorDuplicateKeyword("short");
                    continue;
                }
                isShort = true;
                continue;
            }
            if (NULL != (tmpType = findType(token)))
            {
                // Found the base variable type
                if (NULL != foundType)
                {
                    printError("multiple types specified");
                    free(tmpType);
                    continue;
                }

                foundType = tmpType;
                tmpType   = NULL;

                applyVarDescriptors(isShort, isUnsigned, foundType);

                continue;
            }

            if (isIdentifier(token))
            {
                if (NULL == foundType)
                {
                    printError("expected type before identifier");
                    drainToNextSemicolon();
                    break;
                }
                ident = token;
                token = NULL;
                continue;
            }

            if (isKeyword(token))
            {
                printError("unexpected keyword");
                drainToNextSemicolon();
                continue;
            }

            printErrorArgs("idk what you did, but this is wrong \"%s\"", token);
            drainToNextSemicolon();
        }

        isUnsigned = false;
        isShort    = false;

        if (NULL != ident)
        {
            free(ident);
            ident = NULL;
        }

        if (NULL != token)
        {
            free(token);
            token = NULL;
        }

        if (NULL != tmpType)
        {
            free(tmpType);
            tmpType = NULL;
        }

        if (NULL != foundType)
        {
            free(foundType);
            foundType = NULL;
        }
    }

    return !error;
}

// DEBUG

#define PRINT_DEBUG_BANNER printf("\n========================================"\
                                    "========================================\n\n")

static void DEBUG_printType(type_t *t)
{
    if (NULL == t)
    {
        INTERNAL_ERROR;
        return;
    }

    if (t->isVoid)
    {
        printf("void");
    }
    else if (t->isRaw)
    {
        switch (t->type.rawType)
        {
            case int_e:
            {
                printf("int");
                break;
            }
            case uint_e:
            {
                printf("unsigned int");
                break;
            }
            case hint_e:
            {
                printf("short int");
                break;
            }
            case huint_e:
            {
                printf("unsigned short int");
                break;
            }
            case char_e:
            {
                printf("char");
                break;
            }
            case uchar_e:
            {
                printf("unsigned char");
                break;
            }
            default:
            {
                INTERNAL_ERROR;
                break;
            }
        }
    }
    else if (NULL != t->type.typeDefinition)
    {
        printf("%s", t->type.typeDefinition->identifier);
    }
    else
    {
        INTERNAL_ERROR;
        return;
    }

    for (int i = 0; i < t->pointerDepth; i++)
    {
        printf("*");
    }
}

static void DEBUG_printPadding(int padding)
{
    for (int i = 0; i < padding; i++)
    {
        printf(" ");
    }
}

static void DEBUG_printExpression(expression_t *exp)
{
    if (NULL == exp)
    {
        INTERNAL_ERROR;
        return;
    }

    switch (exp->expressionType)
    {
        case et_variable_e:
        {
            if (NULL == exp->contents.variable)
            {
                printf("NULL VARIABLE");
                return;
            }
            printf("%s", exp->contents.variable->identifier);
            return;
        }
        case et_literal_e:
        {
            printf("%u", exp->contents.literal);
            return;
        }
        case et_stringLiteral_e:
        {
            printf("\"%s\"", exp->contents.stringLiteral);
            return;
        }
        case et_unary_e:
        {
            switch (exp->contents.unary.operation)
            {
                case op_postIncrement_e:
                case op_postDecrement_e:
                {
                    printf("<");
                    DEBUG_printExpression(exp->contents.unary.operand);
                    printf("%s>", operatorTokens[exp->contents.unary.operation]);
                    break;
                }
                case op_preIncrement_e:
                case op_preDecrement_e:
                case op_dereference_e:
                case op_reference_e:
                case op_negative_e:
                {
                    printf("<%s", operatorTokens[exp->contents.unary.operation]);
                    DEBUG_printExpression(exp->contents.unary.operand);
                    printf(">");
                    break;
                }
                case op_parenthesis_e:
                {
                    DEBUG_printExpression(exp->contents.unary.operand);
                    break;
                }
                default:
                {
                    printf("UNARY OPERATOR NOT SUPORTED");
                    return;
                }
            }
            return;
        }
        case et_binary_e:
        {
            printf("<");
            DEBUG_printExpression(exp->contents.binary.operand1);
            printf(" %s ", operatorTokens[exp->contents.binary.operation]);
            if (false == operatorAttatchLeft[exp->contents.binary.operation])
            {
                printf("to ");
            }
            DEBUG_printExpression(exp->contents.binary.operand2);
            printf(">");
            return;
        }
        case et_trinary_e:
        {
            printf("TRINARY NOT IMPLEMENTED");
            return;
        }
        case et_functionCall_e:
        {
            printf("FUNCTION CALL NOT IMPLEMENTED");
            return;
        }
        default:
        {
            INTERNAL_ERROR;
            return;
        }
    }
}

static void DEBUG_printStackFrame(int padding, stackFrame_t *sf)
{
    if (padding < 0 ||
        NULL == sf)
    {
        INTERNAL_ERROR;
        return;
    }

    DEBUG_printPadding(padding);
    printf("variables:\n");
    if (NULL == sf->variables.first)
    {
        DEBUG_printPadding(padding + 2);
        printf("NONE\n");
    }
    else
    {
        for (variable_t *v = sf->variables.first; NULL != v; v = v->next)
        {
            DEBUG_printPadding(padding + 2);
            printf ("%s is of type ", v->identifier);
            DEBUG_printType(&v->type);
            printf("\n");

            if (NULL != v->initializer)
            {
                DEBUG_printPadding(padding + 4);
                printf("initializer: ");
                DEBUG_printExpression(v->initializer);
                printf("\n");
            }
        }
    }
    DEBUG_printPadding(padding);
    printf("code block:\n");
    if (NULL == sf->codeBlock.firstItem)
    {
        DEBUG_printPadding(padding + 2);
        printf("NONE\n");
    }
    else
    {
        for (voidContainer_t *vc = sf->codeBlock.firstItem; NULL != vc; vc = vc->nextVoidContainer)
        {
            DEBUG_printPadding(padding + 2);
            switch (vc->type)
            {
                case stackFrame_e:
                {
                    printf("stack frame:\n");
                    DEBUG_printStackFrame(padding + 4, vc->data);
                    break;
                }
                case expression_e:
                {
                    printf("expression: ");
                    DEBUG_printExpression(vc->data);
                    printf("\n");
                    break;
                }
                case if_e:
                {
                    if_t *i = vc->data;

                    if (i->parent == NULL)
                    {
                        printf("if (");
                        DEBUG_printExpression(i->condition);
                        printf("):\n");
                        DEBUG_printStackFrame(padding + 4, &i->consequence);
                    }
                    else
                    {
                        printf("else");
                        if (i->condition != NULL)
                        {
                            printf(" if (");
                            DEBUG_printExpression(i->condition);
                            printf(")");
                        }
                        printf(":\n");
                        DEBUG_printStackFrame(padding + 4, &i->consequence);
                    }
                    break;
                }
                default:
                {
                    printf("unknown\n");
                }
            }
        }
    }
}

static void printDebug()
{
    PRINT_DEBUG_BANNER;
    printf("Typedefs:\n");
    if (NULL == typedefs.first)
    {
        printf("  NONE\n");
    }
    else
    {
        for (typedef_t *t = typedefs.first; NULL != t; t = t->next)
        {
            printf("  %s is ", t->identifier);
            switch (t->typedefType)
            {
                case typeExtension_e:
                {
                    printf("of type ");
                    DEBUG_printType(&t->content.typeExtension);
                    printf("\n");
                    break;
                }
                case typeStructDefinition_e:
                {
                    printf("a structure <not yet represented>\n");
                    break;
                }
                case typeEnumDefinition_e:
                {
                    printf("an enum <not yet represented>\n");
                    break;
                }
                default:
                {
                    printf("broken\n");
                    break;
                }
            }
        }
    }

    PRINT_DEBUG_BANNER;

    printf("Global variables:\n");
    if (NULL == globalVariables.first)
    {
        printf("  NONE\n");
    }
    else
    {
        for (variable_t *v = globalVariables.first; NULL != v; v = v->next)
        {
            printf ("  %s is of type ", v->identifier);
            DEBUG_printType(&v->type);
            printf("\n");

            if (NULL != v->initializer)
            {
                printf("    initializer: ");
                DEBUG_printExpression(v->initializer);
                printf("\n");
            }
        }
    }

    PRINT_DEBUG_BANNER;

    printf("Functions:\n");
    if (NULL == functions.first)
    {
        printf(" NONE\n");
    }
    else
    {
        for (function_t *f = functions.first; NULL != f; f = f->next)
        {
            printf("  %s:\n", f->identifier);

            printf("    returns ");
            DEBUG_printType(&f->returnType);
            printf("\n");

            printf("    parameters:\n");
            if (NULL == f->parameterNames.first ||
                NULL == f->parameterTypes.first)
            {
                printf("      NONE\n");
            }
            else
            {
                strll_t *i = f->parameterNames.first;
                type_t  *t = f->parameterTypes.first;
                
                while (i != NULL && t != NULL)
                {
                    printf("      ");
                    DEBUG_printType(t);
                    printf(" (called %s)\n", i->str);

                    i = i->next;
                    t = t->next;
                }
            }

            printf("    definition:\n");
            if (false == f->isDefined)
            {
                printf("      NONE\n");
            }
            else
            {
                DEBUG_printStackFrame(6, &f->definition);
            }

            if (NULL != f->next)
            {
                printf("\n");
            }
        }
    }

    PRINT_DEBUG_BANNER;
}
// DEBUG

bool compile(char *inputFileName, char *outputFileName)
{
    if (NULL == inputFileName ||
        NULL == outputFileName)
    {
        INTERNAL_ERROR;
        return false;
    }

    inputFile = fopen(inputFileName, "r");

    if (NULL == inputFile)
    {
        printf("Could not open %s\n", inputFileName);
        return false;
    }

    fileName = strcpy(calloc(strlen(inputFileName) + 1, sizeof(char)),
                      inputFileName);

    if (false == intake())
    {
        fclose(inputFile);
        free(fileName);
        return false;
    }

    fclose(inputFile);
    free(fileName);

    // DEBUG
    printDebug();
    // DEBUG

    return true;
}
