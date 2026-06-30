/* test_numfmt.c - tests for the lightweight number formatting helpers.
 *
 * Learning objective: these are pure functions (no HW, no globals) and the
 * easiest place to see the test pattern — set up inputs, call, ASSERT_* on the
 * output buffer and the returned length.
 *
 * Coverage: ss_utoa_dec / ss_itoa_dec / ss_itoa_dec_pad / ss_utoa_hex,
 * including the INT32_MIN edge case that exercises the (v+1)+1u two's-complement
 * trick in ss_itoa_dec. */

#include "ssos_test.h"
#include "numfmt.h"

#include <stdint.h>
#include <string.h>

/* ---- ss_utoa_dec ---- */

TEST(utoa_dec_basic) {
    char buf[12];
    int n = ss_utoa_dec(0, buf);
    ASSERT_STR_EQ(buf, "0");
    ASSERT_EQ(n, 1);
}

TEST(utoa_dec_small) {
    char buf[12];
    int n = ss_utoa_dec(1, buf);
    ASSERT_STR_EQ(buf, "1");
    ASSERT_EQ(n, 1);
}

TEST(utoa_dec_large) {
    char buf[12];
    int n = ss_utoa_dec(12345, buf);
    ASSERT_STR_EQ(buf, "12345");
    ASSERT_EQ(n, 5);
}

TEST(utoa_dec_max_uint32) {
    char buf[12];
    int n = ss_utoa_dec(0xFFFFFFFFu, buf);
    ASSERT_STR_EQ(buf, "4294967295");
    ASSERT_EQ(n, 10);
}

/* ---- ss_itoa_dec ---- */

TEST(itoa_dec_positive) {
    char buf[12];
    int n = ss_itoa_dec(42, buf);
    ASSERT_STR_EQ(buf, "42");
    ASSERT_EQ(n, 2);
}

TEST(itoa_dec_negative_one) {
    char buf[12];
    int n = ss_itoa_dec(-1, buf);
    ASSERT_STR_EQ(buf, "-1");
    ASSERT_EQ(n, 2);
}

TEST(itoa_dec_int32_min) {
    /* INT32_MIN is the tricky case: -(v+1)+1 avoids UB on negation. */
    char buf[12];
    int n = ss_itoa_dec(INT32_MIN, buf);
    ASSERT_STR_EQ(buf, "-2147483648");
    ASSERT_EQ(n, 11);
}

/* ---- ss_itoa_dec_pad ---- */

TEST(itoa_dec_pad_wider) {
    char buf[16];
    /* "42" padded to width 5 -> 3 leading spaces. */
    int n = ss_itoa_dec_pad(42, buf, 5);
    ASSERT_STR_EQ(buf, "   42");
    ASSERT_EQ(n, 5);
}

TEST(itoa_dec_pad_exact) {
    char buf[16];
    /* width == digit count -> no padding. */
    int n = ss_itoa_dec_pad(123, buf, 3);
    ASSERT_STR_EQ(buf, "123");
    ASSERT_EQ(n, 3);
}

TEST(itoa_dec_pad_narrower) {
    char buf[16];
    /* width smaller than digits -> no truncation, no padding. */
    int n = ss_itoa_dec_pad(12345, buf, 2);
    ASSERT_STR_EQ(buf, "12345");
    ASSERT_EQ(n, 5);
}

TEST(itoa_dec_pad_negative) {
    char buf[16];
    /* "-7" padded to width 4 -> 2 leading spaces, '-' counts toward width. */
    int n = ss_itoa_dec_pad(-7, buf, 4);
    ASSERT_STR_EQ(buf, "  -7");
    ASSERT_EQ(n, 4);
}

/* ---- ss_utoa_hex ---- */

TEST(utoa_hex_zero) {
    char buf[9];
    int n = ss_utoa_hex(0, buf, 0);
    ASSERT_STR_EQ(buf, "0");
    ASSERT_EQ(n, 1);
}

TEST(utoa_hex_padded) {
    char buf[9];
    int n = ss_utoa_hex(0xFFu, buf, 2);
    ASSERT_STR_EQ(buf, "FF");
    ASSERT_EQ(n, 2);
}

TEST(utoa_hex_width8) {
    char buf[9];
    int n = ss_utoa_hex(0x1234ABCDu, buf, 8);
    ASSERT_STR_EQ(buf, "1234ABCD");
    ASSERT_EQ(n, 8);
}

TEST(utoa_hex_width_smaller_than_digits) {
    char buf[9];
    /* width 1 but value needs 3 hex digits -> no truncation, full output. */
    int n = ss_utoa_hex(0xFFFu, buf, 1);
    ASSERT_STR_EQ(buf, "FFF");
    ASSERT_EQ(n, 3);
}

void run_numfmt_tests(void) {
    RUN_TEST(utoa_dec_basic);
    RUN_TEST(utoa_dec_small);
    RUN_TEST(utoa_dec_large);
    RUN_TEST(utoa_dec_max_uint32);
    RUN_TEST(itoa_dec_positive);
    RUN_TEST(itoa_dec_negative_one);
    RUN_TEST(itoa_dec_int32_min);
    RUN_TEST(itoa_dec_pad_wider);
    RUN_TEST(itoa_dec_pad_exact);
    RUN_TEST(itoa_dec_pad_narrower);
    RUN_TEST(itoa_dec_pad_negative);
    RUN_TEST(utoa_hex_zero);
    RUN_TEST(utoa_hex_padded);
    RUN_TEST(utoa_hex_width8);
    RUN_TEST(utoa_hex_width_smaller_than_digits);
}
