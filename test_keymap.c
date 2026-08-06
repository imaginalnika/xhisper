/*
 * test_keymap.c - Unit tests for the ASCII -> Linux keycode mapping.
 * Pure logic tests, no uinput/display required.
 */

#include <stdio.h>
#include <stdint.h>
#include <linux/input-event-codes.h>
#include "keymap.h"

static int checks = 0;
static int failures = 0;

static void check(int32_t got, int32_t expected, const char *layout, unsigned char c) {
    checks++;
    if (got != expected) {
        failures++;
        printf("FAIL: %-7s 0x%02x '%c'  got 0x%08x  want 0x%08x\n",
               layout, c, c >= 32 && c < 127 ? c : '?', got, expected);
    }
}

#define EXPECT(layout, ch, expected) check(keymap_lookup((layout), (ch)), (expected), (layout), (ch))

int main(void) {
    /* US layout must be unchanged from the original hardcoded table. */
    EXPECT("us", 'a', KEY_A);
    EXPECT("us", 'A', KEY_A | FLAG_UPPERCASE);
    EXPECT("us", '1', KEY_1);
    EXPECT("us", '!', KEY_1 | FLAG_UPPERCASE);
    EXPECT("us", '\'', KEY_APOSTROPHE);
    EXPECT("us", '?', KEY_SLASH | FLAG_UPPERCASE);
    EXPECT("us", '"', KEY_APOSTROPHE | FLAG_UPPERCASE);
    EXPECT("us", '@', KEY_2 | FLAG_UPPERCASE);
    EXPECT("us", '^', KEY_6 | FLAG_UPPERCASE);
    EXPECT("us", '~', KEY_GRAVE | FLAG_UPPERCASE);
    EXPECT("us", '`', KEY_GRAVE);
    EXPECT("us", '\\', KEY_BACKSLASH);
    EXPECT("us", '<', KEY_COMMA | FLAG_UPPERCASE);
    EXPECT("us", '{', KEY_LEFTBRACE | FLAG_UPPERCASE);
    EXPECT("us", '|', KEY_BACKSLASH | FLAG_UPPERCASE);
    EXPECT("us", ' ', KEY_SPACE);

    /* Danish layout: the two reported bugs. */
    EXPECT("danish", '\'', KEY_BACKSLASH);                      /* was being typed as o-slash */
    EXPECT("danish", '?', KEY_MINUS | FLAG_UPPERCASE);          /* was being typed as underscore */

    /* Danish layout: letters and digits sit on the same physical keys. */
    EXPECT("danish", 'a', KEY_A);
    EXPECT("danish", 'A', KEY_A | FLAG_UPPERCASE);
    EXPECT("danish", 'z', KEY_Z);
    EXPECT("danish", 'Z', KEY_Z | FLAG_UPPERCASE);
    EXPECT("danish", '1', KEY_1);
    EXPECT("danish", '!', KEY_1 | FLAG_UPPERCASE);

    /* Danish layout: shifted symbols. */
    EXPECT("danish", '"', KEY_2 | FLAG_UPPERCASE);
    EXPECT("danish", '#', KEY_3 | FLAG_UPPERCASE);
    EXPECT("danish", '%', KEY_5 | FLAG_UPPERCASE);
    EXPECT("danish", '&', KEY_6 | FLAG_UPPERCASE);
    EXPECT("danish", '6', KEY_6);
    EXPECT("danish", '/', KEY_7 | FLAG_UPPERCASE);   /* shift+7 */
    EXPECT("danish", '7', KEY_7);
    EXPECT("danish", '(', KEY_8 | FLAG_UPPERCASE);
    EXPECT("danish", ')', KEY_9 | FLAG_UPPERCASE);
    EXPECT("danish", '=', KEY_0 | FLAG_UPPERCASE);
    EXPECT("danish", '+', KEY_MINUS);
    EXPECT("danish", '*', KEY_BACKSLASH | FLAG_UPPERCASE);
    EXPECT("danish", ';', KEY_COMMA | FLAG_UPPERCASE);
    EXPECT("danish", ':', KEY_DOT | FLAG_UPPERCASE);
    EXPECT("danish", '<', KEY_102ND);
    EXPECT("danish", '>', KEY_102ND | FLAG_UPPERCASE);
    EXPECT("danish", '-', KEY_SLASH);
    EXPECT("danish", '_', KEY_SLASH | FLAG_UPPERCASE);

    /* Danish layout: AltGr characters. */
    EXPECT("danish", '@', KEY_2 | FLAG_ALTGR);
    EXPECT("danish", '$', KEY_4 | FLAG_ALTGR);
    EXPECT("danish", '{', KEY_7 | FLAG_ALTGR);
    EXPECT("danish", '[', KEY_8 | FLAG_ALTGR);
    EXPECT("danish", ']', KEY_9 | FLAG_ALTGR);
    EXPECT("danish", '}', KEY_0 | FLAG_ALTGR);
    EXPECT("danish", '\\', KEY_102ND | FLAG_ALTGR);
    EXPECT("danish", '|', KEY_EQUAL | FLAG_ALTGR);

    /* Danish layout: dead keys (emitted as key + space). */
    EXPECT("danish", '`', KEY_EQUAL | FLAG_UPPERCASE | FLAG_DEADKEY);
    EXPECT("danish", '^', KEY_RIGHTBRACE | FLAG_UPPERCASE | FLAG_DEADKEY);
    EXPECT("danish", '~', KEY_RIGHTBRACE | FLAG_ALTGR | FLAG_DEADKEY);

    /* Non-ASCII falls back to the clipboard path in xhisper.sh. */
    EXPECT("us", 0xC3, -1);
    EXPECT("danish", 0xE6, -1);

    /* Unknown layout falls back to us. */
    EXPECT("gibberish", '\'', KEY_APOSTROPHE);
    EXPECT("gibberish", '?', KEY_SLASH | FLAG_UPPERCASE);

    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
