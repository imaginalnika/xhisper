// keymap.c - ASCII to Linux keycode mapping tables for xhisper.
// One table per keyboard layout. Letters and digits share physical keys
// across US/Danish; only the symbol rows differ.

#include <string.h>
#include <linux/input-event-codes.h>
#include "keymap.h"

// US QWERTY mapping. Each entry is either:
//   -1                              (unmapped/unsupported)
//   KEY_* constant                  (unshifted character)
//   KEY_* | FLAG_UPPERCASE          (shifted character)
static const int32_t us_map[128] = {
	// Control characters (0x00-0x1f): mostly unmapped except tab and enter
	-1,-1,-1,-1,-1,-1,-1,-1,  // 0x00-0x07
	-1,KEY_TAB,KEY_ENTER,-1,-1,-1,-1,-1,  // 0x08-0x0f (tab=0x09, enter=0x0a)
	-1,-1,-1,-1,-1,-1,-1,-1,  // 0x10-0x17
	-1,-1,-1,-1,-1,-1,-1,-1,  // 0x18-0x1f

	// Printable characters (0x20-0x7e)
	// Space and symbols (0x20-0x2f)
	KEY_SPACE,                      // 0x20 ' '
	KEY_1|FLAG_UPPERCASE,           // 0x21 '!' (shift+1)
	KEY_APOSTROPHE|FLAG_UPPERCASE,  // 0x22 '"' (shift+')
	KEY_3|FLAG_UPPERCASE,           // 0x23 '#' (shift+3)
	KEY_4|FLAG_UPPERCASE,           // 0x24 '$' (shift+4)
	KEY_5|FLAG_UPPERCASE,           // 0x25 '%' (shift+5)
	KEY_7|FLAG_UPPERCASE,           // 0x26 '&' (shift+7)
	KEY_APOSTROPHE,                 // 0x27 '''
	KEY_9|FLAG_UPPERCASE,           // 0x28 '(' (shift+9)
	KEY_0|FLAG_UPPERCASE,           // 0x29 ')' (shift+0)
	KEY_8|FLAG_UPPERCASE,           // 0x2a '*' (shift+8)
	KEY_EQUAL|FLAG_UPPERCASE,       // 0x2b '+' (shift+=)
	KEY_COMMA,                      // 0x2c ','
	KEY_MINUS,                      // 0x2d '-'
	KEY_DOT,                        // 0x2e '.'
	KEY_SLASH,                      // 0x2f '/'

	// Digits (0x30-0x39)
	KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,

	// More symbols (0x3a-0x40)
	KEY_SEMICOLON|FLAG_UPPERCASE,   // 0x3a ':' (shift+;)
	KEY_SEMICOLON,                  // 0x3b ';'
	KEY_COMMA|FLAG_UPPERCASE,       // 0x3c '<' (shift+,)
	KEY_EQUAL,                      // 0x3d '='
	KEY_DOT|FLAG_UPPERCASE,         // 0x3e '>' (shift+.)
	KEY_SLASH|FLAG_UPPERCASE,       // 0x3f '?' (shift+/)
	KEY_2|FLAG_UPPERCASE,           // 0x40 '@' (shift+2)

	// Uppercase letters (0x41-0x5a): A-Z
	KEY_A|FLAG_UPPERCASE,KEY_B|FLAG_UPPERCASE,KEY_C|FLAG_UPPERCASE,KEY_D|FLAG_UPPERCASE,
	KEY_E|FLAG_UPPERCASE,KEY_F|FLAG_UPPERCASE,KEY_G|FLAG_UPPERCASE,KEY_H|FLAG_UPPERCASE,
	KEY_I|FLAG_UPPERCASE,KEY_J|FLAG_UPPERCASE,KEY_K|FLAG_UPPERCASE,KEY_L|FLAG_UPPERCASE,
	KEY_M|FLAG_UPPERCASE,KEY_N|FLAG_UPPERCASE,KEY_O|FLAG_UPPERCASE,KEY_P|FLAG_UPPERCASE,
	KEY_Q|FLAG_UPPERCASE,KEY_R|FLAG_UPPERCASE,KEY_S|FLAG_UPPERCASE,KEY_T|FLAG_UPPERCASE,
	KEY_U|FLAG_UPPERCASE,KEY_V|FLAG_UPPERCASE,KEY_W|FLAG_UPPERCASE,KEY_X|FLAG_UPPERCASE,
	KEY_Y|FLAG_UPPERCASE,KEY_Z|FLAG_UPPERCASE,

	// Brackets and symbols (0x5b-0x60)
	KEY_LEFTBRACE,                  // 0x5b '['
	KEY_BACKSLASH,                  // 0x5c '\'
	KEY_RIGHTBRACE,                 // 0x5d ']'
	KEY_6|FLAG_UPPERCASE,           // 0x5e '^' (shift+6)
	KEY_MINUS|FLAG_UPPERCASE,       // 0x5f '_' (shift+-)
	KEY_GRAVE,                      // 0x60 '`'

	// Lowercase letters (0x61-0x7a): a-z
	KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,
	KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,
	KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,
	KEY_Y,KEY_Z,

	// Final symbols (0x7b-0x7e)
	KEY_LEFTBRACE|FLAG_UPPERCASE,   // 0x7b '{' (shift+[)
	KEY_BACKSLASH|FLAG_UPPERCASE,   // 0x7c '|' (shift+\)
	KEY_RIGHTBRACE|FLAG_UPPERCASE,  // 0x7d '}' (shift+])
	KEY_GRAVE|FLAG_UPPERCASE,       // 0x7e '~' (shift+`)

	-1  // 0x7f DEL (unmapped)
};

// Danish (ISO) differs from US only on the symbol rows; letters and digits
// sit on the same physical keys. Each override lists one differing ASCII
// char and its Danish key definition, derived from
// /usr/share/X11/xkb/symbols/dk:
//   AE11 '+' '?'  AE12 dead_acute/dead_grave + '|' on AltGr
//   AD11 'a-ring' AD12 dead_diaeresis/^/~ on AltGr
//   AC10 'ae'  AC11 'o-slash'  BKSL '\'' '*'
//   LSGT '<' '>' '\' on AltGr
//   AB08 ',' ';'  AB09 '.' ':'  AB10 '-' '_'
// Non-ASCII keys (a-ring, ae, o-slash, etc.) are typed via clipboard.
struct keymap_override {
	unsigned char c;
	int32_t kdef;
};

static const struct keymap_override danish_overrides[] = {
	{ '"', KEY_2 | FLAG_UPPERCASE },                    // shift+2
	{ '$', KEY_4 | FLAG_ALTGR },                        // AltGr+4
	{ '&', KEY_6 | FLAG_UPPERCASE },                    // shift+6
	{ '\'', KEY_BACKSLASH },                            // Danish apostrophe key
	{ '(', KEY_8 | FLAG_UPPERCASE },                    // shift+8
	{ ')', KEY_9 | FLAG_UPPERCASE },                    // shift+9
	{ '*', KEY_BACKSLASH | FLAG_UPPERCASE },            // shift+'
	{ '+', KEY_MINUS },                                 // Danish + key
	{ '-', KEY_SLASH },                                 // Danish - key
	{ '/', KEY_7 | FLAG_UPPERCASE },                    // shift+7
	{ ':', KEY_DOT | FLAG_UPPERCASE },                  // shift+.
	{ ';', KEY_COMMA | FLAG_UPPERCASE },                // shift+,
	{ '<', KEY_102ND },                                 // Danish <> key
	{ '=', KEY_0 | FLAG_UPPERCASE },                    // shift+0
	{ '>', KEY_102ND | FLAG_UPPERCASE },                // shift+<>
	{ '?', KEY_MINUS | FLAG_UPPERCASE },                // shift++ on AE11
	{ '@', KEY_2 | FLAG_ALTGR },                        // AltGr+2
	{ '[', KEY_8 | FLAG_ALTGR },                        // AltGr+8
	{ '\\', KEY_102ND | FLAG_ALTGR },                   // AltGr+<>
	{ ']', KEY_9 | FLAG_ALTGR },                        // AltGr+9
	{ '^', KEY_RIGHTBRACE | FLAG_UPPERCASE | FLAG_DEADKEY }, // dead ^
	{ '_', KEY_SLASH | FLAG_UPPERCASE },                // shift+-
	{ '`', KEY_EQUAL | FLAG_UPPERCASE | FLAG_DEADKEY }, // dead `
	{ '{', KEY_7 | FLAG_ALTGR },                        // AltGr+7
	{ '|', KEY_EQUAL | FLAG_ALTGR },                    // AltGr+´ key
	{ '}', KEY_0 | FLAG_ALTGR },                        // AltGr+0
	{ '~', KEY_RIGHTBRACE | FLAG_ALTGR | FLAG_DEADKEY }, // dead ~
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// Registry of supported layouts. Each layout's full table is a copy of the
// US base table with its sparse overrides applied on top.
static const struct keymap_layout {
	const char *name;
	const struct keymap_override *overrides;
	size_t n_overrides;
} layouts[] = {
	{ "us", NULL, 0 },
	{ "dk", danish_overrides, ARRAY_LEN(danish_overrides) },
};

// Resolved full tables, one per layouts[] entry, built lazily on first use.
static int32_t maps[ARRAY_LEN(layouts)][128];

int32_t keymap_lookup(const char *layout, unsigned char c) {
    if (c >= 128) return -1;
    if (!layout) layout = "us";

    // Build every layout table once on first use. The daemon is
    // single-threaded, so a single static flag is safe.
    static int resolved = 0;
    size_t i, j;
    if (!resolved) {
        for (i = 0; i < ARRAY_LEN(layouts); i++) {
            memcpy(maps[i], us_map, sizeof(us_map));
            for (j = 0; j < layouts[i].n_overrides; j++) {
                maps[i][layouts[i].overrides[j].c] = layouts[i].overrides[j].kdef;
            }
        }
        resolved = 1;
    }

    for (i = 0; i < ARRAY_LEN(layouts); i++) {
        if (strcmp(layout, layouts[i].name) == 0) {
            return maps[i][c];
        }
    }
    return maps[0][c]; // unknown layout falls back to us
}
