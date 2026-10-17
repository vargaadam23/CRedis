// Unit tests for src/utils.c (strToLong).
//
// strToLong parses a base-10 integer that must be followed directly by the
// CRLF delimiter, e.g. "123\r\n". Same harness idea as test_clrf.c: every test
// runs in a forked child so a crash only fails that one test.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#include "utils.h"

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            fprintf(stderr, "      CHECK failed (line %d): %s\n", __LINE__, #cond); \
            fflush(stderr);                                                         \
            _exit(1);                                                               \
        }                                                                           \
    } while (0)

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

// strToLong takes a non-const char*, so give it a writable copy.
static ConversionResult conv(const char *input) {
    char *copy = strdup(input);
    if (!copy) _exit(2);
    ConversionResult r = strToLong(copy);
    free(copy);
    return r;
}

// ------------------------------------------------------------ valid input

static void test_positive(void) {
    ConversionResult r = conv("123\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 123);
}

static void test_negative(void) {
    ConversionResult r = conv("-456\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == -456);
}

static void test_zero(void) {
    ConversionResult r = conv("0\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 0);
}

static void test_negative_zero(void) {
    ConversionResult r = conv("-0\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 0);
}

static void test_explicit_plus_sign(void) {
    ConversionResult r = conv("+7\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 7);
}

static void test_leading_zeros(void) {
    ConversionResult r = conv("007\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 7);
}

static void test_data_after_delimiter_is_ignored(void) {
    ConversionResult r = conv("12\r\n:34\r\n");
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 12);
}

static void test_long_max(void) {
    char buf[64];
    snprintf(buf, sizeof buf, "%ld\r\n", LONG_MAX);
    ConversionResult r = conv(buf);
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == LONG_MAX);
}

static void test_long_min(void) {
    char buf[64];
    snprintf(buf, sizeof buf, "%ld\r\n", LONG_MIN);
    ConversionResult r = conv(buf);
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == LONG_MIN);
}

static void test_stale_errno_is_ignored(void) {
    errno = ERANGE;
    char *s = strdup("5\r\n");
    ConversionResult r = strToLong(s);
    CHECK(r.error == PARSE_OK);
    CHECK(r.result == 5);
}

// ---------------------------------------------------------- invalid input

static void test_empty_string(void) {
    ConversionResult r = conv("");
    CHECK(r.error != PARSE_OK);
    CHECK(r.result == 0);
}

static void test_only_delimiter(void) {
    ConversionResult r = conv("\r\n");
    CHECK(r.error == PARSE_ERR_BAD_NUMBER);
    CHECK(r.result == 0);
}

static void test_no_digits(void) {
    ConversionResult r = conv("abc\r\n");
    CHECK(r.error == PARSE_ERR_BAD_NUMBER);
    CHECK(r.result == 0);
}

static void test_sign_only(void) {
    ConversionResult r = conv("-\r\n");
    CHECK(r.error == PARSE_ERR_BAD_NUMBER);
}

static void test_double_sign(void) {
    ConversionResult r = conv("--5\r\n");
    CHECK(r.error != PARSE_OK);
}

static void test_trailing_garbage(void) {
    ConversionResult r = conv("12abc\r\n");
    CHECK(r.error != PARSE_OK);
    CHECK(r.result == 0);
}

static void test_embedded_space(void) {
    ConversionResult r = conv("1 2\r\n");
    CHECK(r.error != PARSE_OK);
}

static void test_decimal_point(void) {
    ConversionResult r = conv("1.5\r\n");
    CHECK(r.error != PARSE_OK);
}

static void test_hex_is_not_accepted(void) {
    ConversionResult r = conv("0x10\r\n");
    CHECK(r.error != PARSE_OK);
}

static void test_missing_delimiter(void) {
    ConversionResult r = conv("123");
    CHECK(r.error != PARSE_OK);
    CHECK(r.result == 0);
}

static void test_cr_without_lf(void) {
    ConversionResult r = conv("123\r");
    CHECK(r.error != PARSE_OK);
}

static void test_lf_without_cr(void) {
    ConversionResult r = conv("123\n");
    CHECK(r.error != PARSE_OK);
}

static void test_lf_before_cr(void) {
    ConversionResult r = conv("123\n\r");
    CHECK(r.error != PARSE_OK);
}

// edge: strtol silently skips leading whitespace, RESP does not allow it.
static void test_leading_space(void) {
    ConversionResult r = conv(" 12\r\n");
    CHECK(r.error != PARSE_OK);
}

static void test_leading_newline(void) {
    ConversionResult r = conv("\n12\r\n");
    CHECK(r.error != PARSE_OK);
}

// edge: out-of-range values must be reported, with result left at 0.
static void test_overflow_positive(void) {
    ConversionResult r = conv("99999999999999999999\r\n");
    CHECK(r.error != PARSE_OK);
    CHECK(r.result == 0);
}

static void test_overflow_negative(void) {
    ConversionResult r = conv("-99999999999999999999\r\n");
    CHECK(r.error != PARSE_OK);
    CHECK(r.result == 0);
}

// edge: one past LONG_MAX (ERANGE, strtol clamps to LONG_MAX).
static void test_one_past_long_max(void) {
    ConversionResult r = conv("9223372036854775808\r\n");
    CHECK(r.error != PARSE_OK);
    CHECK(r.result == 0);
}

// edge: an out-of-range number is a bad number, not a syntax problem.
static void test_overflow_reports_bad_number(void) {
    ConversionResult r = conv("99999999999999999999\r\n");
    CHECK(r.error == PARSE_ERR_BAD_NUMBER);
}

// ----------------------------------------------------------------- runner

#define T(fn) { #fn, fn }

static const TestCase tests[] = {
    T(test_positive), T(test_negative), T(test_zero), T(test_negative_zero),
    T(test_explicit_plus_sign), T(test_leading_zeros),
    T(test_data_after_delimiter_is_ignored), T(test_long_max), T(test_long_min),
    T(test_stale_errno_is_ignored),

    T(test_empty_string), T(test_only_delimiter), T(test_no_digits),
    T(test_sign_only), T(test_double_sign), T(test_trailing_garbage),
    T(test_embedded_space), T(test_decimal_point), T(test_hex_is_not_accepted),
    T(test_missing_delimiter), T(test_cr_without_lf), T(test_lf_without_cr),
    T(test_lf_before_cr), T(test_leading_space), T(test_leading_newline),

    T(test_overflow_positive), T(test_overflow_negative),
    T(test_one_past_long_max), T(test_overflow_reports_bad_number),
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
