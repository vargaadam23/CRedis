#include "clrf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int freeObject(Object *object)
{
    int i;

    for (i = 0; i < object->count; i++)
    {
        freeValue(object->keys[i]);
        freeValue(object->values[i]);
    }

    free(object);
}
// Double nest check
int freeArray(Array *array)
{
    int i;

    for (i = 0; i < array->count; i++)
    {
        freeValue(array->items[i]);
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

char* clrfToValue(char *value, Value *out)
{
    size_t valueSize = sizeof(value);
    char *end;

    if (valueSize < 2)
    {
        errno = PARSE_ERR_EMPTY_STR;
        return NULL;
    }

    ValueType valueType = getValueType(value[0]);

    if (valueType == INVALID)
    {
        errno = PARSE_ERR_SYNTAX;
        return NULL;
    }

    out->type = valueType;
    char *start = value + 1;

    if (*start == '\0' || start == NULL)
    {
        errno = PARSE_ERR_UNEXPECTED_END;
        return NULL;
    }

    switch (valueType)
    {
    case VAL_INT:
        end = strstr(start, "\r\n");

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        out->as.i = strtol(start, (char **)NULL, 10);
        break;
    case VAL_STRING:
        start = strstr(start, "\r\n") + strlen("\r\n");

        if (*start == '\0' || start == NULL)
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        end = strstr(start, "\r\n");

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        if (end - start < 0)
        {
            errno = PARSE_ERR_EMPTY_STR;
            return NULL;
        }

        out->as.s = malloc(end - start + 1);

        memcpy(out->as.s, start, end - start);
        break;
    case VAL_SIMPLE_STR:
        end = strstr(start, "\r\n");

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        out->as.ss = malloc(end - start + 1);

        memcpy(out->as.ss, start, end - start);
        break;
    case VAL_ERR:
        end = strstr(start, "\r\n");

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        out->as.err = malloc(end - start + 1);

        memcpy(out->as.err, start, end - start);
        break;
    case VAL_STRUCT:
        end = strstr(start, "\r\n");

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        long number_of_entries = strtol(start, (char **)NULL, 10);

        Object *map = malloc(sizeof(Object));

        map->count = 0;
        map->capacity = number_of_entries;

        map->keys = calloc(number_of_entries, sizeof(struct Value *));
        map->values = calloc(number_of_entries, sizeof(struct Value *));

        int entry_number;

        end = end + 2;

        for (entry_number = 0; entry_number < number_of_entries; entry_number++)
        {
            Value *current_key = malloc(sizeof(struct Value));
            Value *current_value = malloc(sizeof(struct Value));

            char* current_end = clrfToValue(end, current_key);
            end = clrfToValue(current_end, current_value);
            printf("Value is %d", current_value->as.i);

            map->keys[entry_number] = current_key;
            map->values[entry_number] = current_value;
            map->count++;
        }

        out->as.obj = map;

        break;
    case VAL_ARRAY:
        end = strstr(start, "\r\n");

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        long number_ofa_entries = strtol(start, (char **)NULL, 10);

        Array *array = malloc(sizeof(Array));

        array->count = 0;
        array->capacity = number_ofa_entries;

        array->items = calloc(number_ofa_entries, sizeof(struct Value));

        int aentry_number;

        end = end + 2;

        for (aentry_number = 0; aentry_number < number_ofa_entries; aentry_number++)
        {
            Value *current_value = malloc(sizeof(struct Value));

            end = clrfToValue(end, current_value);

            array->items[aentry_number] = current_value;
            array->count++;
        }

        out->as.arr = array;

        break;
    default:
        errno = PARSE_ERR_SYNTAX;
        return NULL;
    }

    return end + 2;
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