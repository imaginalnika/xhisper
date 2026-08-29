/*
 * test_xhispertool.c — unit tests for xhispertool logic
 *
 * Tests the ASCII keycode mapping and character classification logic.
 * Included directly from xhispertool.c to access static internals without
 * requiring uinput or sockets — a standard C pattern for unit testing
 * static functions.
 */

#include "testutil.h"
#include <stdint.h>

/* Pull in the source under test. We stub out the hardware-dependent parts
 * so the logic can run without /dev/uinput or a running daemon. */
#define TESTING
/* Prevent xhispertool.c from defining main() */
#define main xhispertool_main_unused
#include "../xhispertool.c"
#undef main

/* ── ascii2keycode_map tests ─────────────────────────────────────────────── */

void test_printable_ascii_range_is_mapped(int *failed) {
    for (int c = 32; c <= 126; c++) {
        ASSERT_TRUE(
            ascii2keycode_map[c] != -1,
            "printable ASCII char is mapped",
            failed
        );
    }
}

void test_control_chars_are_unmapped(int *failed) {
    /* Control characters (except tab=0x09, enter=0x0a) should be -1 */
    for (int c = 0; c <= 8; c++) {
        ASSERT_EQ(-1, ascii2keycode_map[c], "control char 0x00-0x08 unmapped", failed);
    }
    for (int c = 11; c <= 31; c++) {
        ASSERT_EQ(-1, ascii2keycode_map[c], "control char 0x0b-0x1f unmapped", failed);
    }
}

void test_tab_and_enter_are_mapped(int *failed) {
    ASSERT_TRUE(ascii2keycode_map['\t'] != -1, "tab (0x09) is mapped", failed);
    ASSERT_TRUE(ascii2keycode_map['\n'] != -1, "enter (0x0a) is mapped", failed);
}

void test_del_is_unmapped(int *failed) {
    ASSERT_EQ(-1, ascii2keycode_map[127], "DEL (0x7f) is unmapped", failed);
}

void test_uppercase_letters_have_shift_flag(int *failed) {
    for (int c = 'A'; c <= 'Z'; c++) {
        ASSERT_TRUE(
            ascii2keycode_map[c] & FLAG_UPPERCASE,
            "uppercase letter has FLAG_UPPERCASE",
            failed
        );
    }
}

void test_lowercase_letters_have_no_shift_flag(int *failed) {
    for (int c = 'a'; c <= 'z'; c++) {
        ASSERT_FALSE(
            ascii2keycode_map[c] & FLAG_UPPERCASE,
            "lowercase letter has no FLAG_UPPERCASE",
            failed
        );
    }
}

void test_digits_have_no_shift_flag(int *failed) {
    for (int c = '0'; c <= '9'; c++) {
        ASSERT_FALSE(
            ascii2keycode_map[c] & FLAG_UPPERCASE,
            "digit has no FLAG_UPPERCASE",
            failed
        );
    }
}

void test_shifted_symbols_have_shift_flag(int *failed) {
    /* These symbols require Shift on a US QWERTY layout */
    const char shifted[] = "!@#$%^&*()_+{}|:\"<>?~";
    for (size_t i = 0; i < sizeof(shifted) - 1; i++) {
        unsigned char c = (unsigned char)shifted[i];
        ASSERT_TRUE(
            ascii2keycode_map[c] & FLAG_UPPERCASE,
            "shifted symbol has FLAG_UPPERCASE",
            failed
        );
    }
}

void test_unshifted_symbols_have_no_shift_flag(int *failed) {
    /* These symbols do not require Shift on a US QWERTY layout */
    const char unshifted[] = "`-=[]\\;',./";
    for (size_t i = 0; i < sizeof(unshifted) - 1; i++) {
        unsigned char c = (unsigned char)unshifted[i];
        ASSERT_FALSE(
            ascii2keycode_map[c] & FLAG_UPPERCASE,
            "unshifted symbol has no FLAG_UPPERCASE",
            failed
        );
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    int failed = 0;

    TEST_SUITE_BEGIN("xhispertool");

    test_printable_ascii_range_is_mapped(&failed);
    test_control_chars_are_unmapped(&failed);
    test_tab_and_enter_are_mapped(&failed);
    test_del_is_unmapped(&failed);
    test_uppercase_letters_have_shift_flag(&failed);
    test_lowercase_letters_have_no_shift_flag(&failed);
    test_digits_have_no_shift_flag(&failed);
    test_shifted_symbols_have_shift_flag(&failed);
    test_unshifted_symbols_have_no_shift_flag(&failed);

    TEST_SUITE_END(failed);
}
