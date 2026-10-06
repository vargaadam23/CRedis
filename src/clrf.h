#include<stddef.h>

#define MAX_BUFFER 1024

typedef enum {
    PARSE_OK = 0,
    PARSE_ERR_SYNTAX,
    PARSE_ERR_UNEXPECTED_END,
    PARSE_ERR_BAD_NUMBER,
    PARSE_ERR_UNTERMINATED_STRING,
    PARSE_ERR_OUT_OF_MEMORY,
    PARSE_ERR_EMPTY_STR
} ParseError;

typedef enum {
    VAL_INT,
    VAL_STRING,
    VAL_BOOL,
    VAL_ARRAY,
    VAL_STRUCT,
    VAL_SIMPLE_STR,
    VAL_ERR,
    VAL_NULL,
    VAL_DOUBLE,
    INVALID
} ValueType;

typedef struct Value Value;

typedef struct {
    Value  *items;   // heap array of Value
    size_t  count;
    size_t  capacity;
} Array;

typedef struct {
    char  **keys;    // keys[i] pairs with values[i]
    Value  *values;
    size_t  count;
    size_t  capacity;
} Object;

struct Value {
    ValueType type;
    union {
        long    i;
        char   *s;      // heap-allocated, owned by the Value
        bool    b;
        Array   *arr;
        Object  *obj;
        int null;
        double d;
        char *ss;
        char *err;
    } as;
};

ParseError clrfToValue(char* value, Value *out);

char* valueToClrf(Value *input);

ValueType getValueType(char type);


int freeObject(Object *object);

int freeArray(Array *array);

int freeValue(Value *value);
