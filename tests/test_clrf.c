// Unit tests for src/clrf.c (RESP parsing / serialisation).
//
// Every test runs in its own forked child process, so a test that crashes
// (SIGSEGV, SIGABRT...) is reported as a failure of that test only, instead
// of killing the whole test binary.
//
// Tests are split in two groups:
//   - "behaves"  : functionality that is expected to work today
//   - "edge"     : edge cases taken from the RESP spec / C pitfalls; many of
//                  these are EXPECTED TO FAIL against the current
//                  implementation, that is the point.
//
// Note: inputs are always heap copies (heap_copy()) because clrfToValue writes into
// the buffer it is given, so string literals (read-only memory) are unsafe.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/wait.h>

#include "constants.h"
#include "clrf.h"
#include "common.h"

// ---------------------------------------------------------------- harness

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            fprintf(stderr, "      CHECK failed (line %d): %s\n", __LINE__, #cond); \
            fflush(stderr);                                                      \
            _exit(1);                                                            \
        }                                                                        \
    } while (0)

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

static char *heap_copy(const char *s) {
    char *copy = strdup(s);
    if (!copy) _exit(2);
    return copy;
}

static Value *new_value(void) {
    // malloc + memset (not calloc): calloc bypasses glibc's tcache and would
    // skew the leak measurements below.
    Value *v = malloc(sizeof(Value));
    if (!v) _exit(2);
    memset(v, 0, sizeof(Value));
    return v;
}

// Parse `input` (copied to the heap). Returns what clrfToValue returned and
// fills *out_value / *out_input so tests can check pointers into the input.
static char *parse(const char *input, Value **out_value, char **out_input) {
    *out_input = heap_copy(input);
    *out_value = new_value();
    errno = 0;
    return clrfToValue(*out_input, *out_value);
}

static bool str_is(const char *actual, const char *expected) {
    return actual != NULL && strcmp(actual, expected) == 0;
}

// =========================================================================
// getValueType / getValueTypeNotation
// =========================================================================

static void test_get_value_type_known_prefixes(void) {
    CHECK(getValueType('+') == VAL_SIMPLE_STR);
    CHECK(getValueType('-') == VAL_ERR);
    CHECK(getValueType(':') == VAL_INT);
    CHECK(getValueType('$') == VAL_STRING);
    CHECK(getValueType('*') == VAL_ARRAY);
    CHECK(getValueType('%') == VAL_STRUCT);
    CHECK(getValueType('_') == VAL_NULL);
    CHECK(getValueType(',') == VAL_DOUBLE);
}

static void test_get_value_type_unknown_prefixes(void) {
    CHECK(getValueType('?') == INVALID);
    CHECK(getValueType('a') == INVALID);
    CHECK(getValueType('\0') == INVALID);
    CHECK(getValueType('\r') == INVALID);
    CHECK(getValueType((char)0xFF) == INVALID);
}

static void test_notation_for_basic_types(void) {
    CHECK(getValueTypeNotation(VAL_SIMPLE_STR) == '+');
    CHECK(getValueTypeNotation(VAL_ERR) == '-');
    CHECK(getValueTypeNotation(VAL_INT) == ':');
    CHECK(getValueTypeNotation(VAL_STRING) == '$');
    CHECK(getValueTypeNotation(VAL_ARRAY) == '*');
    CHECK(getValueTypeNotation(VAL_STRUCT) == '%');
}

// edge: every prefix that getValueType accepts should map back to itself.
static void test_type_and_notation_round_trip(void) {
    const char prefixes[] = "+-:$*%_,";
    for (const char *p = prefixes; *p; p++) {
        ValueType t = getValueType(*p);
        CHECK(t != INVALID);
        if (getValueTypeNotation(t) != *p) {
            fprintf(stderr, "      prefix '%c' -> type %d -> notation '%c'\n",
                    *p, t, getValueTypeNotation(t) ? getValueTypeNotation(t) : '0');
            _exit(1);
        }
    }
}

// =========================================================================
// clrfToValue: integers
// =========================================================================

static void test_parse_int(void) {
    Value *v; char *in;
    char *end = parse(":42\r\n", &v, &in);
    CHECK(end == in + 5);
    CHECK(v->type == VAL_INT);
    CHECK(v->as.i == 42);
}

static void test_parse_negative_int(void) {
    Value *v; char *in;
    char *end = parse(":-1000\r\n", &v, &in);
    CHECK(end == in + 8);
    CHECK(v->as.i == -1000);
}

static void test_parse_zero(void) {
    Value *v; char *in;
    CHECK(parse(":0\r\n", &v, &in) != NULL);
    CHECK(v->as.i == 0);
}

static void test_parse_int_with_plus_sign(void) {
    Value *v; char *in;
    CHECK(parse(":+7\r\n", &v, &in) != NULL);
    CHECK(v->as.i == 7);
}

static void test_parse_int_missing_crlf(void) {
    Value *v; char *in;
    CHECK(parse(":42", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_UNEXPECTED_END);
}

static void test_parse_int_only_cr_no_lf(void) {
    Value *v; char *in;
    CHECK(parse(":42\r", &v, &in) == NULL);
}

static void test_parse_int_non_numeric(void) {
    Value *v; char *in;
    CHECK(parse(":abc\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_BAD_NUMBER);
}

static void test_parse_int_trailing_garbage(void) {
    Value *v; char *in;
    CHECK(parse(":12abc\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_BAD_NUMBER);
}

static void test_parse_int_empty_body(void) {
    Value *v; char *in;
    CHECK(parse(":\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_BAD_NUMBER);
}

static void test_parse_int_overflow(void) {
    Value *v; char *in;
    // larger than LONG_MAX on 64-bit
    CHECK(parse(":99999999999999999999\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_BAD_NUMBER);
}

static void test_parse_int_returns_pointer_to_next_value(void) {
    Value *v; char *in;
    char *end = parse(":1\r\n:2\r\n", &v, &in);
    CHECK(end != NULL);
    CHECK(strcmp(end, ":2\r\n") == 0);
}

// =========================================================================
// clrfToValue: simple strings and errors
// =========================================================================

static void test_parse_simple_string(void) {
    Value *v; char *in;
    char *end = parse("+OK\r\n", &v, &in);
    CHECK(end == in + 5);
    CHECK(v->type == VAL_SIMPLE_STR);
    CHECK(strncmp(v->as.ss, "OK", 2) == 0);
}

static void test_parse_error(void) {
    Value *v; char *in;
    char *end = parse("-ERR unknown command\r\n", &v, &in);
    CHECK(end == in + 22);
    CHECK(v->type == VAL_ERR);
    CHECK(strncmp(v->as.err, "ERR unknown command", 19) == 0);
}

static void test_parse_empty_simple_string(void) {
    Value *v; char *in;
    char *end = parse("+\r\n", &v, &in);
    CHECK(end == in + 3);
    CHECK(v->type == VAL_SIMPLE_STR);
}

static void test_parse_simple_string_missing_crlf(void) {
    Value *v; char *in;
    CHECK(parse("+OK", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_UNEXPECTED_END);
}

static void test_parse_simple_string_lf_only(void) {
    Value *v; char *in;
    CHECK(parse("+OK\n", &v, &in) == NULL);
}

static void test_parse_simple_string_followed_by_other_value(void) {
    Value *v; char *in;
    char *end = parse("+OK\r\n+NEXT\r\n", &v, &in);
    CHECK(end != NULL);
    CHECK(strcmp(end, "+NEXT\r\n") == 0);
}

// edge: as.ss is a C string, so it must be NUL terminated. malloc() gives
// uninitialised memory, so we poison a freed block of the same size class
// first to make the missing terminator visible.
static void test_simple_string_is_nul_terminated(void) {
    char *in = heap_copy("+OK\r\n");
    Value *v = new_value();
    char *poison = malloc(16);
    memset(poison, 0xAA, 16);
    free(poison);
    CHECK(clrfToValue(in, v) != NULL);
    CHECK(str_is(v->as.ss, "OK"));
}

static void test_error_is_nul_terminated(void) {
    char *in = heap_copy("-ERR bad\r\n");
    Value *v = new_value();
    char *poison = malloc(16);
    memset(poison, 0xAA, 16);
    free(poison);
    CHECK(clrfToValue(in, v) != NULL);
    CHECK(str_is(v->as.err, "ERR bad"));
}

// =========================================================================
// clrfToValue: bulk strings
// =========================================================================

static void test_parse_bulk_string(void) {
    Value *v; char *in;
    char *end = parse("$5\r\nhello\r\n", &v, &in);
    CHECK(end == in + 11);
    CHECK(v->type == VAL_STRING);
    CHECK(strncmp(v->as.s, "hello", 5) == 0);
}

static void test_parse_empty_bulk_string(void) {
    Value *v; char *in;
    char *end = parse("$0\r\n\r\n", &v, &in);
    CHECK(end == in + 6);
    CHECK(v->type == VAL_STRING);
}

static void test_parse_bulk_string_missing_final_crlf(void) {
    Value *v; char *in;
    CHECK(parse("$5\r\nhello", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_UNEXPECTED_END);
}

static void test_parse_bulk_string_missing_header_crlf(void) {
    Value *v; char *in;
    CHECK(parse("$5", &v, &in) == NULL);
}

static void test_parse_bulk_string_followed_by_other_value(void) {
    Value *v; char *in;
    char *end = parse("$3\r\nfoo\r\n:1\r\n", &v, &in);
    CHECK(end != NULL);
    CHECK(strcmp(end, ":1\r\n") == 0);
}

static void test_bulk_string_is_nul_terminated(void) {
    char *in = heap_copy("$5\r\nhello\r\n");
    Value *v = new_value();
    char *poison = malloc(16);
    memset(poison, 0xAA, 16);
    free(poison);
    CHECK(clrfToValue(in, v) != NULL);
    CHECK(str_is(v->as.s, "hello"));
}

// edge: the declared length is part of the protocol; it must match the body.
static void test_bulk_string_length_longer_than_body(void) {
    Value *v; char *in;
    // says 10 bytes, only 5 present
    CHECK(parse("$10\r\nhello\r\n", &v, &in) == NULL);
}

static void test_bulk_string_length_shorter_than_body(void) {
    Value *v; char *in;
    // says 3 bytes, body is 5
    CHECK(parse("$3\r\nhello\r\n", &v, &in) == NULL);
}

// edge: bulk strings are binary safe, the body may contain \r\n.
static void test_bulk_string_body_containing_crlf(void) {
    Value *v; char *in;
    char *end = parse("$4\r\na\r\nb\r\n", &v, &in);
    CHECK(end == in + 10);
    CHECK(v->type == VAL_STRING);
    CHECK(memcmp(v->as.s, "a\r\nb", 4) == 0);
}

// edge: "$-1\r\n" is the RESP2 null bulk string.
static void test_null_bulk_string(void) {
    Value *v; char *in;
    char *end = parse("$-1\r\n", &v, &in);
    CHECK(end == in + 5);
    CHECK(v->type == VAL_NULL);
}

static void test_bulk_string_negative_length_other_than_minus_one(void) {
    Value *v; char *in;
    CHECK(parse("$-5\r\nhello\r\n", &v, &in) == NULL);
}

static void test_bulk_string_non_numeric_length(void) {
    Value *v; char *in;
    CHECK(parse("$abc\r\nhello\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_BAD_NUMBER);
}

// =========================================================================
// clrfToValue: arrays
// =========================================================================

static void test_parse_array_of_ints(void) {
    Value *v; char *in;
    char *end = parse("*2\r\n:1\r\n:2\r\n", &v, &in);
    CHECK(end == in + 12);
    CHECK(v->type == VAL_ARRAY);
    CHECK(v->as.arr->count == 2);
    CHECK(v->as.arr->items[0]->as.i == 1);
    CHECK(v->as.arr->items[1]->as.i == 2);
}

static void test_parse_empty_array(void) {
    Value *v; char *in;
    char *end = parse("*0\r\n", &v, &in);
    CHECK(end == in + 4);
    CHECK(v->type == VAL_ARRAY);
    CHECK(v->as.arr->count == 0);
}

// The shape of a real Redis command: ECHO hey
static void test_parse_array_of_bulk_strings(void) {
    Value *v; char *in;
    char *end = parse("*2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n", &v, &in);
    CHECK(end == in + 23);
    CHECK(v->as.arr->count == 2);
    CHECK(v->as.arr->items[0]->type == VAL_STRING);
    CHECK(strncmp(v->as.arr->items[0]->as.s, "ECHO", 4) == 0);
    CHECK(strncmp(v->as.arr->items[1]->as.s, "hey", 3) == 0);
}

static void test_parse_array_mixed_types(void) {
    Value *v; char *in;
    CHECK(parse("*3\r\n+OK\r\n:5\r\n$2\r\nhi\r\n", &v, &in) != NULL);
    CHECK(v->as.arr->count == 3);
    CHECK(v->as.arr->items[0]->type == VAL_SIMPLE_STR);
    CHECK(v->as.arr->items[1]->type == VAL_INT);
    CHECK(v->as.arr->items[2]->type == VAL_STRING);
}

static void test_parse_nested_array(void) {
    Value *v; char *in;
    char *end = parse("*2\r\n*1\r\n:1\r\n:2\r\n", &v, &in);
    CHECK(end == in + 17);
    CHECK(v->as.arr->count == 2);
    CHECK(v->as.arr->items[0]->type == VAL_ARRAY);
    CHECK(v->as.arr->items[0]->as.arr->items[0]->as.i == 1);
    CHECK(v->as.arr->items[1]->as.i == 2);
}

static void test_parse_array_followed_by_other_value(void) {
    Value *v; char *in;
    char *end = parse("*1\r\n:1\r\n+NEXT\r\n", &v, &in);
    CHECK(end != NULL);
    CHECK(strcmp(end, "+NEXT\r\n") == 0);
}

static void test_parse_array_missing_header_crlf(void) {
    Value *v; char *in;
    CHECK(parse("*2", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_UNEXPECTED_END);
}

// edge: header promises 2 elements, only 1 is there. Must be an error and
// the returned pointer must be NULL (not some garbage non-NULL pointer).
static void test_parse_array_fewer_elements_than_declared(void) {
    Value *v; char *in;
    char *end = parse("*2\r\n:1\r\n", &v, &in);
    CHECK(end == NULL);
}

static void test_parse_array_no_elements_but_count_declared(void) {
    Value *v; char *in;
    char *end = parse("*3\r\n", &v, &in);
    CHECK(end == NULL);
}

// edge: an element that is malformed must fail the whole array.
static void test_parse_array_with_invalid_element(void) {
    Value *v; char *in;
    CHECK(parse("*2\r\n:1\r\n?bad\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_SYNTAX);
}

static void test_parse_array_with_invalid_element_first(void) {
    Value *v; char *in;
    CHECK(parse("*2\r\n?bad\r\n:1\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_SYNTAX);
}

// edge: "*-1\r\n" is the RESP2 null array.
static void test_null_array(void) {
    Value *v; char *in;
    char *end = parse("*-1\r\n", &v, &in);
    CHECK(end == in + 5);
    CHECK(v->type == VAL_NULL);
}

static void test_parse_array_non_numeric_count(void) {
    Value *v; char *in;
    CHECK(parse("*abc\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_BAD_NUMBER);
}

// =========================================================================
// clrfToValue: maps (RESP3 '%')
// =========================================================================

static void test_parse_map(void) {
    Value *v; char *in;
    char *end = parse("%2\r\n+a\r\n:1\r\n+b\r\n:2\r\n", &v, &in);
    CHECK(end == in + 22);
    CHECK(v->type == VAL_STRUCT);
    CHECK(v->as.obj->count == 2);
    CHECK(v->as.obj->keys[0]->type == VAL_SIMPLE_STR);
    CHECK(v->as.obj->values[0]->as.i == 1);
    CHECK(v->as.obj->values[1]->as.i == 2);
}

static void test_parse_empty_map(void) {
    Value *v; char *in;
    char *end = parse("%0\r\n", &v, &in);
    CHECK(end == in + 4);
    CHECK(v->as.obj->count == 0);
}

// edge: header says 2 pairs, only 1 present.
static void test_parse_map_fewer_pairs_than_declared(void) {
    Value *v; char *in;
    CHECK(parse("%2\r\n+a\r\n:1\r\n", &v, &in) == NULL);
}

// edge: a key without its value.
static void test_parse_map_missing_value(void) {
    Value *v; char *in;
    CHECK(parse("%1\r\n+a\r\n", &v, &in) == NULL);
}

// =========================================================================
// clrfToValue: other RESP3 types and generic input problems
// =========================================================================

// edge: RESP3 null is "_\r\n".
static void test_parse_resp3_null(void) {
    Value *v; char *in;
    char *end = parse("_\r\n", &v, &in);
    CHECK(end == in + 3);
    CHECK(v->type == VAL_NULL);
}

// edge: RESP3 double is ",<float>\r\n".
static void test_parse_double(void) {
    Value *v; char *in;
    char *end = parse(",3.14\r\n", &v, &in);
    CHECK(end == in + 7);
    CHECK(v->type == VAL_DOUBLE);
    CHECK(v->as.d > 3.139 && v->as.d < 3.141);
}

static void test_parse_unknown_type_byte(void) {
    Value *v; char *in;
    CHECK(parse("?foo\r\n", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_SYNTAX);
}

static void test_parse_type_byte_only(void) {
    Value *v; char *in;
    CHECK(parse("+", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_UNEXPECTED_END);
}

// edge: empty input should be reported as an empty string, not as a syntax
// error about an unknown type byte.
static void test_parse_empty_input(void) {
    Value *v; char *in;
    CHECK(parse("", &v, &in) == NULL);
    CHECK(errno == PARSE_ERR_EMPTY_STR);
}

static void test_parse_input_over_length_limit(void) {
    size_t len = MAX_PARSED_STRING_LENGTH + 100;
    char *in = malloc(len + 1);
    CHECK(in != NULL);
    in[0] = '+';
    memset(in + 1, 'a', len - 3);
    in[len - 2] = '\r';
    in[len - 1] = '\n';
    in[len] = '\0';
    Value *v = new_value();
    errno = 0;
    CHECK(clrfToValue(in, v) == NULL);
    CHECK(errno == PARSE_ERR_STRING_LIMIT);
}

// =========================================================================
// valueToClrf
// =========================================================================

static void test_serialize_simple_string(void) {
    Value v = { .type = VAL_SIMPLE_STR };
    v.as.ss = "OK";
    char *out = valueToClrf(&v);
    CHECK(str_is(out, "+OK\r\n"));
}

static void test_serialize_error(void) {
    Value v = { .type = VAL_ERR };
    v.as.err = "ERR x";
    char *out = valueToClrf(&v);
    CHECK(str_is(out, "-ERR x\r\n"));
}

static void test_serialize_bulk_string(void) {
    Value v = { .type = VAL_STRING };
    v.as.s = "hello";
    char *out = valueToClrf(&v);
    CHECK(str_is(out, "$5\r\nhello\r\n"));
}

static void test_serialize_empty_bulk_string(void) {
    Value v = { .type = VAL_STRING };
    v.as.s = "";
    char *out = valueToClrf(&v);
    CHECK(str_is(out, "$0\r\n\r\n"));
}

static void test_serialize_unsupported_type_returns_null(void) {
    Value v = { .type = INVALID };
    CHECK(valueToClrf(&v) == NULL);
}

// edge: output buffers must be large enough for the whole content.
static void test_serialize_long_simple_string(void) {
    Value v = { .type = VAL_SIMPLE_STR };
    v.as.ss = "this is a fairly long simple string, well over a few bytes";
    char *out = valueToClrf(&v);
    CHECK(str_is(out, "+this is a fairly long simple string, well over a few bytes\r\n"));
}

static void test_serialize_long_error(void) {
    Value v = { .type = VAL_ERR };
    v.as.err = "ERR this error message is definitely longer than a pointer";
    char *out = valueToClrf(&v);
    CHECK(str_is(out, "-ERR this error message is definitely longer than a pointer\r\n"));
}

static void test_serialize_long_bulk_string(void) {
    char body[201];
    memset(body, 'x', 200);
    body[200] = '\0';
    Value v = { .type = VAL_STRING };
    v.as.s = body;
    char *out = valueToClrf(&v);
    CHECK(out != NULL);
    CHECK(strlen(out) == 1 + 3 + 2 + 200 + 2);
    CHECK(strncmp(out, "$200\r\n", 6) == 0);
    CHECK(strcmp(out + 6 + 200, "\r\n") == 0);
}

// edge: integers are a basic RESP type and should be serialisable.
static void test_serialize_int(void) {
    Value v = { .type = VAL_INT };
    v.as.i = 42;
    char *out = valueToClrf(&v);
    CHECK(str_is(out, ":42\r\n"));
}

static void test_serialize_negative_int(void) {
    Value v = { .type = VAL_INT };
    v.as.i = -7;
    char *out = valueToClrf(&v);
    CHECK(str_is(out, ":-7\r\n"));
}

// parse(serialize(x)) == x
static void test_round_trip_bulk_string(void) {
    Value v = { .type = VAL_STRING };
    v.as.s = "round trip";
    char *wire = valueToClrf(&v);
    CHECK(wire != NULL);
    Value *back = new_value();
    CHECK(clrfToValue(wire, back) != NULL);
    CHECK(back->type == VAL_STRING);
    CHECK(strncmp(back->as.s, "round trip", 10) == 0);
}

// =========================================================================
// makePrimitiveValue
// =========================================================================

static void test_make_int_value(void) {
    long n = 99;
    errno = 0;
    Value *v = makePrimitiveValue(VAL_INT, &n);
    CHECK(v != NULL);
    CHECK(v->type == VAL_INT);
    CHECK(v->as.i == 99);
}

static void test_make_string_value(void) {
    char *s = heap_copy("abc");
    errno = 0;
    Value *v = makePrimitiveValue(VAL_STRING, s);
    CHECK(v != NULL);
    CHECK(v->type == VAL_STRING);
    CHECK(v->as.s == s);
}

static void test_make_simple_string_value(void) {
    char *s = heap_copy("OK");
    errno = 0;
    Value *v = makePrimitiveValue(VAL_SIMPLE_STR, s);
    CHECK(v != NULL);
    CHECK(v->type == VAL_SIMPLE_STR);
    CHECK(v->as.ss == s);
}

static void test_make_error_value(void) {
    char *s = heap_copy("ERR");
    errno = 0;
    Value *v = makePrimitiveValue(VAL_ERR, s);
    CHECK(v != NULL);
    CHECK(v->type == VAL_ERR);
    CHECK(v->as.err == s);
}

static void test_make_value_invalid_type(void) {
    long n = 1;
    CHECK(makePrimitiveValue(INVALID, &n) == NULL);
}

static void test_make_value_null_pointer(void) {
    CHECK(makePrimitiveValue(VAL_INT, NULL) == NULL);
    CHECK(makePrimitiveValue(VAL_STRING, NULL) == NULL);
}

static void test_make_value_unsupported_type(void) {
    long n = 1;
    CHECK(makePrimitiveValue(VAL_ARRAY, &n) == NULL);
}

// edge: errno is "sticky": it keeps whatever an earlier, unrelated failure
// left in it. A successful allocation must not be reported as an error just
// because errno was already non-zero.
static void test_make_value_ignores_stale_errno(void) {
    long n = 5;
    errno = ENOENT;
    Value *v = makePrimitiveValue(VAL_INT, &n);
    CHECK(v != NULL);
    CHECK(v->as.i == 5);
}

// =========================================================================
// memory: freeValue must release everything clrfToValue allocated
// =========================================================================

// glibc's mallinfo2().uordblks = bytes currently handed out by malloc. After a
// warm-up (so allocator caches are primed) one more parse+free cycle must not
// change it. A difference means something was leaked.
static void expect_no_leak(const char *wire) {
    char *in = heap_copy(wire);
    for (int i = 0; i < 10; i++) {
        Value *v = new_value();
        clrfToValue(in, v);
        freeValue(v);
    }
    size_t before = mallinfo2().uordblks;
    Value *v = new_value();
    CHECK(clrfToValue(in, v) != NULL);
    freeValue(v);
    size_t after = mallinfo2().uordblks;
    if (after != before) {
        fprintf(stderr, "      leaked %zd bytes per parse+free cycle\n",
                (ssize_t)(after - before));
        _exit(1);
    }
}

static void test_free_int_does_not_leak(void)           { expect_no_leak(":1\r\n"); }
static void test_free_simple_string_does_not_leak(void) { expect_no_leak("+OK\r\n"); }
static void test_free_bulk_string_does_not_leak(void)   { expect_no_leak("$5\r\nhello\r\n"); }
static void test_free_array_does_not_leak(void)         { expect_no_leak("*2\r\n:1\r\n:2\r\n"); }
static void test_free_nested_array_does_not_leak(void)  { expect_no_leak("*1\r\n*1\r\n:1\r\n"); }
static void test_free_map_does_not_leak(void)           { expect_no_leak("%1\r\n+a\r\n:1\r\n"); }

// =========================================================================
// runner
// =========================================================================

#define T(fn) { #fn, fn }

static const TestCase tests[] = {
    // getValueType / getValueTypeNotation
    T(test_get_value_type_known_prefixes),
    T(test_get_value_type_unknown_prefixes),
    T(test_notation_for_basic_types),
    T(test_type_and_notation_round_trip),

    // ints
    T(test_parse_int),
    T(test_parse_negative_int),
    T(test_parse_zero),
    T(test_parse_int_with_plus_sign),
    T(test_parse_int_missing_crlf),
    T(test_parse_int_only_cr_no_lf),
    T(test_parse_int_returns_pointer_to_next_value),
    T(test_parse_int_non_numeric),
    T(test_parse_int_trailing_garbage),
    T(test_parse_int_empty_body),
    T(test_parse_int_overflow),

    // simple strings / errors
    T(test_parse_simple_string),
    T(test_parse_error),
    T(test_parse_empty_simple_string),
    T(test_parse_simple_string_missing_crlf),
    T(test_parse_simple_string_lf_only),
    T(test_parse_simple_string_followed_by_other_value),
    T(test_simple_string_is_nul_terminated),
    T(test_error_is_nul_terminated),

    // bulk strings
    T(test_parse_bulk_string),
    T(test_parse_empty_bulk_string),
    T(test_parse_bulk_string_missing_final_crlf),
    T(test_parse_bulk_string_missing_header_crlf),
    T(test_parse_bulk_string_followed_by_other_value),
    T(test_bulk_string_is_nul_terminated),
    T(test_bulk_string_length_longer_than_body),
    T(test_bulk_string_length_shorter_than_body),
    T(test_bulk_string_body_containing_crlf),
    T(test_null_bulk_string),
    T(test_bulk_string_negative_length_other_than_minus_one),
    T(test_bulk_string_non_numeric_length),

    // arrays
    T(test_parse_array_of_ints),
    T(test_parse_empty_array),
    T(test_parse_array_of_bulk_strings),
    T(test_parse_array_mixed_types),
    T(test_parse_nested_array),
    T(test_parse_array_followed_by_other_value),
    T(test_parse_array_missing_header_crlf),
    T(test_parse_array_fewer_elements_than_declared),
    T(test_parse_array_no_elements_but_count_declared),
    T(test_parse_array_with_invalid_element),
    T(test_parse_array_with_invalid_element_first),
    T(test_null_array),
    T(test_parse_array_non_numeric_count),

    // maps
    T(test_parse_map),
    T(test_parse_empty_map),
    T(test_parse_map_fewer_pairs_than_declared),
    T(test_parse_map_missing_value),

    // other / generic
    T(test_parse_resp3_null),
    T(test_parse_double),
    T(test_parse_unknown_type_byte),
    T(test_parse_type_byte_only),
    T(test_parse_empty_input),
    T(test_parse_input_over_length_limit),

    // valueToClrf
    T(test_serialize_simple_string),
    T(test_serialize_error),
    T(test_serialize_bulk_string),
    T(test_serialize_empty_bulk_string),
    T(test_serialize_unsupported_type_returns_null),
    T(test_serialize_long_simple_string),
    T(test_serialize_long_error),
    T(test_serialize_long_bulk_string),
    T(test_serialize_int),
    T(test_serialize_negative_int),
    T(test_round_trip_bulk_string),

    // makePrimitiveValue
    T(test_make_int_value),
    T(test_make_string_value),
    T(test_make_simple_string_value),
    T(test_make_error_value),
    T(test_make_value_invalid_type),
    T(test_make_value_null_pointer),
    T(test_make_value_unsupported_type),
    T(test_make_value_ignores_stale_errno),

    // memory
    T(test_free_int_does_not_leak),
    T(test_free_simple_string_does_not_leak),
    T(test_free_bulk_string_does_not_leak),
    T(test_free_array_does_not_leak),
    T(test_free_nested_array_does_not_leak),
    T(test_free_map_does_not_leak),
};

int main(void) {
    size_t total = sizeof(tests) / sizeof(tests[0]);
    size_t passed = 0;

    for (size_t i = 0; i < total; i++) {
        fflush(NULL);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 2;
        }
        if (pid == 0) {
            // Silence the parser's own printf debugging so output stays readable.
            FILE *devnull = freopen("/dev/null", "w", stdout);
            (void)devnull;
            tests[i].fn();
            _exit(0);
        }

        int status = 0;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            passed++;
            printf("[ PASS ] %s\n", tests[i].name);
        } else if (WIFSIGNALED(status)) {
            printf("[ FAIL ] %s (crashed: signal %d)\n", tests[i].name, WTERMSIG(status));
        } else {
            printf("[ FAIL ] %s\n", tests[i].name);
        }
    }

    printf("\n%zu/%zu passed, %zu failed\n", passed, total, total - passed);
    return passed == total ? 0 : 1;
}
