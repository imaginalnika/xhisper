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
    EXPECT("dk", '\'', KEY_BACKSLASH);                      /* was being typed as o-slash */
    EXPECT("dk", '?', KEY_MINUS | FLAG_UPPERCASE);          /* was being typed as underscore */

    /* Danish layout: letters and digits sit on the same physical keys. */
    EXPECT("dk", 'a', KEY_A);
    EXPECT("dk", 'A', KEY_A | FLAG_UPPERCASE);
    EXPECT("dk", 'z', KEY_Z);
    EXPECT("dk", 'Z', KEY_Z | FLAG_UPPERCASE);
    EXPECT("dk", '1', KEY_1);
    EXPECT("dk", '!', KEY_1 | FLAG_UPPERCASE);

    /* Danish layout: shifted symbols. */
    EXPECT("dk", '"', KEY_2 | FLAG_UPPERCASE);
    EXPECT("dk", '#', KEY_3 | FLAG_UPPERCASE);
    EXPECT("dk", '%', KEY_5 | FLAG_UPPERCASE);
    EXPECT("dk", '&', KEY_6 | FLAG_UPPERCASE);
    EXPECT("dk", '6', KEY_6);
    EXPECT("dk", '/', KEY_7 | FLAG_UPPERCASE);   /* shift+7 */
    EXPECT("dk", '7', KEY_7);
    EXPECT("dk", '(', KEY_8 | FLAG_UPPERCASE);
    EXPECT("dk", ')', KEY_9 | FLAG_UPPERCASE);
    EXPECT("dk", '=', KEY_0 | FLAG_UPPERCASE);
    EXPECT("dk", '+', KEY_MINUS);
    EXPECT("dk", '*', KEY_BACKSLASH | FLAG_UPPERCASE);
    EXPECT("dk", ';', KEY_COMMA | FLAG_UPPERCASE);
    EXPECT("dk", ':', KEY_DOT | FLAG_UPPERCASE);
    EXPECT("dk", '<', KEY_102ND);
    EXPECT("dk", '>', KEY_102ND | FLAG_UPPERCASE);
    EXPECT("dk", '-', KEY_SLASH);
    EXPECT("dk", '_', KEY_SLASH | FLAG_UPPERCASE);

    /* Danish layout: AltGr characters. */
    EXPECT("dk", '@', KEY_2 | FLAG_ALTGR);
    EXPECT("dk", '$', KEY_4 | FLAG_ALTGR);
    EXPECT("dk", '{', KEY_7 | FLAG_ALTGR);
    EXPECT("dk", '[', KEY_8 | FLAG_ALTGR);
    EXPECT("dk", ']', KEY_9 | FLAG_ALTGR);
    EXPECT("dk", '}', KEY_0 | FLAG_ALTGR);
    EXPECT("dk", '\\', KEY_102ND | FLAG_ALTGR);
    EXPECT("dk", '|', KEY_EQUAL | FLAG_ALTGR);

    /* Danish layout: dead keys (emitted as key + space). */
    EXPECT("dk", '`', KEY_EQUAL | FLAG_UPPERCASE | FLAG_DEADKEY);
    EXPECT("dk", '^', KEY_RIGHTBRACE | FLAG_UPPERCASE | FLAG_DEADKEY);
    EXPECT("dk", '~', KEY_RIGHTBRACE | FLAG_ALTGR | FLAG_DEADKEY);

    /* Non-ASCII falls back to the clipboard path in xhisper.sh. */
    EXPECT("us", 0xC3, -1);
    EXPECT("dk", 0xE6, -1);

    /* Unknown layout falls back to us. */
    EXPECT("gibberish", '\'', KEY_APOSTROPHE);
    EXPECT("gibberish", '?', KEY_SLASH | FLAG_UPPERCASE);

    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
