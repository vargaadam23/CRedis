#include "common.h"

typedef struct
{
    long result;
    ParseError error;
} ConversionResult;

ConversionResult strToLong(char *stringValue);
