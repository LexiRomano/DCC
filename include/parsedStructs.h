#ifndef __PARSED_STRUCTS_H__
#define __PARSED_STRUCTS_H__

#include <stdbool.h>

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

typedef struct strll_t
{
    char           *str;
    struct strll_t *next;
} strll_t;

typedef struct
{
    strll_t *first;
    strll_t *last;
    int      count;
} stringLinkedList_t;

typedef enum
{
    codeBlock_e,
    function_e,
    stackFrame_e,
    expression_e,
    variable_e,
    typedef_e,
    type_e,
    if_e,
    assembly_e,
    return_e
} ident_e;

typedef enum
{
    int_e,
    uint_e,
    hint_e,
    huint_e,
    char_e,
    uchar_e
} rawType_e;

typedef enum
{
    // 1
    op_parenthesis_e,
    op_variable_e,
    op_literal_e,

    // 2
    op_arrayIndexing_e,
    op_functionCall_e,
    op_memberAccess_e,
    op_memberAccessPt_e,
    op_postIncrement_e,
    op_postDecrement_e,

    // 3
    op_preIncrement_e,
    op_preDecrement_e,
    op_reference_e,
    op_dereference_e,
    op_bitwiseNot_e,
    op_logicalNot_e,
    op_negative_e,

    // 4
    op_cast_e,
    
    // 5
    op_multiplication_e,
    op_division_e,
    op_modulus_e,

    // 6
    op_subtraction_e,
    op_addition_e,

    // 7
    op_bitshiftLeft_e,
    op_bitshiftRight_e,

    // 8
    op_lessThan_e,
    op_greaterThan_e,
    op_lessEqual_e,
    op_greaterEqual_e,

    // 9
    op_equals_e,
    op_notEquals_e,
    
    // 10
    op_bitwiseAnd_e,

    // 11
    op_bitwiseXor_e,

    // 12
    op_bitwiseOr_e,

    // 13
    op_logicalAnd_e,

    // 14
    op_logicalOr_e,

    // 15
    op_conditional_e,

    // 16
    op_assignment_e,
    op_addAssignment_e,
    op_subtractAssignment_e,
    op_multiplyAssignment_e,
    op_divideAssignment_e,
    op_modulusAssignment_e,

    op_count_e, // sentinel
} operation_e;

// forawrd declaration
struct typedef_t;
struct expression_t;

typedef struct type_t
{
    bool isVoid;
    bool isRaw;
    union
    {
        rawType_e rawType;
        struct typedef_t *typeDefinition;
    } type;
    unsigned char pointerDepth;
    unsigned int  size;
    struct type_t *next;
} type_t;

typedef struct
{
    type_t *first;
    type_t *last;
} typeList_t;

typedef struct voidContainer_t
{
    void                   *data;
    ident_e                 type;
    struct voidContainer_t *nextVoidContainer;
} voidContainer_t;

typedef struct
{
    voidContainer_t *firstItem;
    voidContainer_t *lastItem;
} voidList_t;

typedef struct variable_t
{
    char                *identifier;
    type_t               type;
    struct expression_t *initializer;
    int                  offsetInFunc;

    struct variable_t *next;
} variable_t;

typedef struct
{
    variable_t *first;
    variable_t *last;
} variableList_t;

typedef voidList_t codeBlock_t;

typedef struct stackFrame_t
{
    struct stackFrame_t *prevAccessableStackFrame;
    codeBlock_t          codeBlock;
    variableList_t       variables;
    unsigned int         varSize;
    unsigned int         maxStackSize;
    unsigned int         offsetInFunc;
} stackFrame_t;

typedef struct function_t
{
    char              *identifier;
    type_t             returnType;
    typeList_t         parameterTypes;
    stringLinkedList_t parameterNames;
    
    bool               isDefined;
    bool               isUsed;
    stackFrame_t       definition;
    stringLinkedList_t requiredSymbols;

    struct function_t *next;
} function_t;

typedef struct
{
    function_t *first;
    function_t *last;
} functionList_t;

typedef struct
{
    type_t *firstStructEntry;
    type_t *lastStructEntry;
} struct_t;

typedef struct enumEntry_t
{
    char               *identifier;
    int                 value;
    struct enumEntry_t *next;
} enumEntry_t;

typedef struct
{
    char        *identifier;
    enumEntry_t *firstEntry;
    enumEntry_t *lastEntry;
} enum_t;

typedef enum
{
    typeExtension_e,
    typeStructDefinition_e,
    typeEnumDefinition_e 
} typedefType_e;

typedef struct typedef_t
{
    char         *identifier;
    typedefType_e typedefType;
    bool          simplifiesToRawType;
    union
    {
        type_t   typeExtension;
        struct_t *structDefinition;
        enum_t   *enumDefinition;
    } content;

    unsigned int size;

    struct typedef_t *next;
} typedef_t;

typedef struct
{
    typedef_t *first;
    typedef_t *last;
} typedefs_t;

typedef enum
{
    et_variable_e,
    et_literal_e,
    et_stringLiteral_e,
    et_unary_e,
    et_binary_e,
    et_trinary_e,
    et_functionCall_e,
} expressionType_e;

typedef struct
{
    operation_e operation;
    struct expression_t *operand;
} unaryOperator_t;

typedef struct
{
    operation_e operation;
    struct expression_t *operand1;
    struct expression_t *operand2;
} binaryOperator_t;

typedef struct
{
    operation_e operation;
    struct expression_t *operand1;
    struct expression_t *operand2;
    struct expression_t *operand3;
} trinaryOperator_t;

typedef struct
{
    function_t *function;

    struct expression_t *firstParam;
    struct expression_t *lastParam;
} functionCall_t;

typedef struct expression_t
{
    struct expression_t *parent;
    expressionType_e     expressionType;
    type_t               resultingType;

    union
    {
        variable_t       *variable;
        unsigned int      literal;
        char             *stringLiteral;
        unaryOperator_t   unary;
        binaryOperator_t  binary;
        trinaryOperator_t trinary;
        functionCall_t    functionCall;
    } contents;

    struct expression_t *next;
} expression_t;


typedef struct if_t
{
    struct if_t *parent;

    expression_t *condition;
    stackFrame_t  consequence;

    struct if_t *next;
} if_t;

typedef struct
{
    expression_t *value;
} return_t;

typedef struct
{
    typedefs_t     typedefs;
    variableList_t globalVariables;
    functionList_t functions;
} parsedData_t;

typedef stringLinkedList_t assembly_t;

#endif //__PARSED_STRUCTS_H__
