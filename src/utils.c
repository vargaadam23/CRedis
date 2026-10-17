#include "clrf.h"
#include "utils.h"
#include "constants.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

ConversionResult strToLong(char *stringValue)
{
    ConversionResult result;

    char *endpointer;

    errno = 0;

    long value = strtol(stringValue, &endpointer, 10);

    if (endpointer == stringValue)
    {
        result.error = PARSE_ERR_BAD_NUMBER;
        result.result = 0;

        return result;
    }

    if (strncmp(endpointer, CLRF_DELIMITER, strlen(CLRF_DELIMITER)) != 0)
    {
        result.error = PARSE_ERR_SYNTAX;
        result.result = 0;

        return result;
    }

    if (errno != 0)
    {
        result.error = PARSE_ERR_SYNTAX;
        result.result = 0;

        return result;
    }

    result.error = PARSE_OK;
    result.result = value;

    return result;
}