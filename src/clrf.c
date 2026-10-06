#include "clrf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int freeObject(Object *object)
{
    int i;

    for (i = 0; i < object->count; i++)
    {
        free(object->keys + (i * sizeof(char *)));
        freeValue(object->values + (i * sizeof(struct Value)));
    }

    free(object);
}
// Double nest check
int freeArray(Array *array)
{
    int i;

    for (i = 0; i < array->count; i++)
    {
        freeValue(array->items + (i * sizeof(struct Value)));
    }

    free(array);

    return 1;
}

int freeValue(Value *value)
{
    switch (value->type)
    {
    case VAL_SIMPLE_STR:
        free(value->as.ss);
        break;
    case VAL_ERR:
        free(value->as.err);
        break;
    case VAL_STRING:
        free(value->as.s);
        break;
    case VAL_ARRAY:
        freeArray(value->as.arr);
        break;
    case VAL_STRUCT:
        freeObject(value->as.obj);
        break;
    default:
        break;
    }

    free(value);

    return 1;
}

ParseError clrfToValue(char *value, Value *out)
{
    size_t valueSize = sizeof(value);
    char *end;

    if (valueSize < 2)
    {
        return PARSE_ERR_EMPTY_STR;
    }

    ValueType valueType = getValueType(value[0]);

    if (valueType == INVALID)
    {
        return PARSE_ERR_SYNTAX;
    }

    out->type = valueType;
    char *start = value + 1;

    if (*start == '\0' || start == NULL)
    {
        return PARSE_ERR_UNEXPECTED_END;
    }

    switch (valueType)
    {
    case VAL_INT:
        end = strstr(start, "\r\n");

        if (*end == '\0' || end == NULL)
        {
            return PARSE_ERR_UNEXPECTED_END;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        out->as.i = strtol(start, (char **)NULL, 10);
        break;
    case VAL_STRING:
        start = strstr(start, "\r\n") + strlen("\r\n");

        if (*start == '\0' || start == NULL)
        {
            return PARSE_ERR_UNEXPECTED_END;
        }

        end = strstr(start, "\r\n");

        if (*end == '\0' || end == NULL)
        {
            return PARSE_ERR_UNEXPECTED_END;
        }

        if (end - start < 0)
        {
            return PARSE_ERR_EMPTY_STR;
        }

        out->as.s = malloc(end - start + 1);

        memcpy(out->as.s, start, end - start);
        break;
    case VAL_SIMPLE_STR:
        end = strstr(start, "\r\n");

        if (*end == '\0' || end == NULL)
        {
            return PARSE_ERR_UNEXPECTED_END;
        }

        // reuse the start pointer, copy the numeric string in it
        out->as.ss = malloc(end - start + 1);

        memcpy(out->as.ss, start, end - start);
        break;
    case VAL_ERR:
        end = strstr(start, "\r\n");

        if (*end == '\0' || end == NULL)
        {
            return PARSE_ERR_UNEXPECTED_END;
        }

        // reuse the start pointer, copy the numeric string in it
        out->as.err = malloc(end - start + 1);

        memcpy(out->as.err, start, end - start);
        break;
    default:
        return PARSE_ERR_SYNTAX;
    }

    return PARSE_OK;
}

ValueType getValueType(char type)
{
    switch (type)
    {
    case '+':
        return VAL_SIMPLE_STR;
    case '-':
        return VAL_ERR;
    case ':':
        return VAL_INT;
    case '$':
        return VAL_STRING;
    case '*':
        return VAL_ARRAY;
    case '_':
        return VAL_NULL;
    case ',':
        return VAL_DOUBLE;
    case '%':
        return VAL_STRUCT;
    default:
        return INVALID;
    }
}

char *valueToClrf(Value *value)
{
    return "hi";
}