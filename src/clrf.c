#include "clrf.h"
#include "utils.h"
#include "common.h"
#include "constants.h"
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

    free(object->keys);
    free(object->values);
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

    free(array->items);
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

char *clrfToValue(char *value, Value *out)
{
    size_t valueSize = sizeof(value);
    char *end;
    ConversionResult cr;

    if (valueSize < 2)
    {
        errno = PARSE_ERR_EMPTY_STR;
        return NULL;
    }

    if (valueSize > MAX_PARSED_STRING_LENGTH)
    {
        errno = PARSE_ERR_STRING_LIMIT;
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

    if (start == NULL || *start == '\0')
    {
        errno = PARSE_ERR_UNEXPECTED_END;
        return NULL;
    }

    switch (valueType)
    {
    case VAL_INT:
        end = strstr(start, CLRF_DELIMITER);

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        cr = strToLong(start);

        if(cr.error != PARSE_OK) {
            errno = cr.error;
            return NULL;
        }

        out->as.i = cr.result;
        break;
    case VAL_STRING:
        char *strLengthEnd = strstr(start, CLRF_DELIMITER);
        char strLength[LONG_TO_CHAR_BUFFER] = "";

        if (strLengthEnd == NULL || *strLengthEnd == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        memcpy(strLength, start, strLengthEnd - start);

        cr = strToLong(start);

        if(cr.error != PARSE_OK) {
            errno = cr.error;
            return NULL;
        }

        long length = cr.result;

        if (length <= 0)
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        start = strLengthEnd + strlen(CLRF_DELIMITER);

        if (start == NULL || *start == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        end = start + length;

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

        if (strncmp(end, CLRF_DELIMITER, strlen(CLRF_DELIMITER)) != 0) {
            errno = PARSE_ERR_SYNTAX;
            return NULL;
        }

        out->as.s = malloc(end - start + 1);

        memcpy(out->as.s, start, end - start);
        break;
    case VAL_SIMPLE_STR:
        end = strstr(start, CLRF_DELIMITER);

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
        end = strstr(start, CLRF_DELIMITER);

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
        end = strstr(start, CLRF_DELIMITER);

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        cr = strToLong(start);

        if(cr.error != PARSE_OK) {
            errno = cr.error;
            return NULL;
        }

        long number_of_entries = cr.result;

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

            char *current_end = clrfToValue(end, current_key);

            if (current_end == NULL)
            {
                int free_entry_num = 0;
                while (map->keys[free_entry_num] != NULL && map->values[free_entry_num] != NULL && free_entry_num < map->count)
                {
                    freeValue(map->keys[free_entry_num]);
                    freeValue(map->values[free_entry_num]);
                    free_entry_num++;
                }

                free(current_key);
                free(current_value);

                free(map->keys);
                free(map->values);
                free(map);
                return NULL;
            }

            end = clrfToValue(current_end, current_value);

            if (end == NULL)
            {
                int free_entry_num = 0;
                while (map->keys[free_entry_num] != NULL && map->values[free_entry_num] != NULL && free_entry_num < map->count)
                {
                    freeValue(map->keys[free_entry_num]);
                    freeValue(map->values[free_entry_num]);
                    free_entry_num++;
                }

                free(current_key);
                free(current_value);

                free(map->keys);
                free(map->values);
                free(map);
                return NULL;
            }
            printf("Value is %d", current_value->as.i);

            map->keys[entry_number] = current_key;
            map->values[entry_number] = current_value;
            map->count++;
        }

        out->as.obj = map;

        break;
    case VAL_ARRAY:
        end = strstr(start, CLRF_DELIMITER);

        if (end == NULL || *end == '\0')
        {
            errno = PARSE_ERR_UNEXPECTED_END;
            return NULL;
        }

        // reuse the start pointer, copy the numeric string in it
        memcpy(start, start, end - start);

        cr = strToLong(start);

        if(cr.error != PARSE_OK) {
            errno = cr.error;
            return NULL;
        }

        long number_ofa_entries = cr.result;

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

            if (end == NULL)
            {
                int free_entry_num = 0;
                while (array->items[free_entry_num] != NULL && free_entry_num < array->count)
                {
                    freeValue(array->items[free_entry_num]);
                    free_entry_num++;
                }

                free(current_value);

                free(array->items);
                free(array);
                return NULL;
            }

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

char getValueTypeNotation(ValueType type)
{
    switch (type)
    {
    case VAL_SIMPLE_STR:
        return '+';
    case VAL_ERR:
        return '-';
    case VAL_STRING:
        return '$';
    case VAL_INT:
        return ':';
    case VAL_ARRAY:
        return '*';
    case VAL_STRUCT:
        return '%';
    default:
        return '\0';
    }
}

Value *makePrimitiveValue(ValueType type, void *valuePointer)
{
    if (type == INVALID)
    {
        return NULL;
    }

    if (valuePointer == NULL)
    {
        return NULL;
    }

    Value *result = malloc(sizeof(struct Value));

    if (errno != 0)
    {
        printf("Error after allocation - %s", strerror(errno));
        return NULL;
    }

    result->type = type;

    switch (type)
    {
    case VAL_SIMPLE_STR:
        result->as.ss = (char *)valuePointer;
        break;
    case VAL_ERR:
        result->as.err = (char *)valuePointer;
        break;
    case VAL_STRING:
        result->as.s = (char *)valuePointer;
        break;
    case VAL_INT:
        long *ptrToValue = (long *)valuePointer;
        if (ptrToValue == NULL)
        {
            return NULL;
        }
        result->as.i = *ptrToValue;
        break;
    default:
        return NULL;
    }

    return result;
}

char *valueToClrf(Value *value)
{
    char *result;
    size_t resultLength = 0;
    char notation = getValueTypeNotation(value->type);

    switch (value->type)
    {
    case VAL_SIMPLE_STR:
        resultLength = sizeof(notation) + sizeof(value->as.ss) + sizeof(CLRF_DELIMITER) + 1;
        result = malloc(resultLength);

        snprintf(result, resultLength, "%c%s%s", notation, value->as.ss, CLRF_DELIMITER);
        break;
    case VAL_ERR:
        resultLength = sizeof(notation) + sizeof(value->as.err) + sizeof(CLRF_DELIMITER) + 1;
        result = malloc(resultLength);
        snprintf(result, resultLength, "%c%s%s", notation, value->as.err, CLRF_DELIMITER);
        break;
    case VAL_STRING:
        char string_length[LONG_TO_CHAR_BUFFER];
        sprintf(string_length, "%lu", strlen(value->as.s));

        // Length actually bigger then the string TODO: FIX
        resultLength = sizeof(notation) + LONG_TO_CHAR_BUFFER + sizeof(CLRF_DELIMITER) + sizeof(value->as.s) + sizeof(CLRF_DELIMITER) + 1;
        result = malloc(resultLength);

        snprintf(result, resultLength, "%c%s%s%s%s", notation, string_length, CLRF_DELIMITER, value->as.err, CLRF_DELIMITER);
        break;
    default:
        return NULL;
    }

    return result;
}