#ifndef __PARSED_STRUCTS_H__
#define __PARSED_STRUCTS_H__

#include "dcc.h"

typedef enum
{
    codeBlock_e,
    function_e,
    stackFrame_e,
    expression_e,
    variable_e,
    typedef_e
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

struct typedef_t; // forawrd declaration
typedef struct
{
    bool isRaw;
    union
    {
        rawType_e rawType;
        struct typedef_t *typeDefinition;
    } type;
    unsigned char pointerDepth;
} type_t;

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
    char   *identifier;
    type_t  type;

    struct variable_t *next;
} variable_t;

typedef struct
{
    variable_t *first;
    variable_t *last;
} variableList_t;

typedef struct expression_t
{
    variable_t          *lhs;
    struct expression_t *nextExpression;
} expression_t;

typedef voidList_t codeBlock_t;

typedef struct
{
    char  *identifier;
    type_t returnType;

} function_t;

typedef struct
{
    codeBlock_t *codeBlock;
} stackFrame_t;

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
    union
    {
        type_t   typeExtension;
        struct_t *structDefinition;
        enum_t   *enumDefinition;
    } content;

    struct typedef_t *next;
} typedef_t;

typedef struct
{
    typedef_t *first;
    typedef_t *last;
} typedefs_t;

#endif //__PARSED_STRUCTS_H__
