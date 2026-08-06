// keymap.h - ASCII to Linux keycode mapping for xhisper.
// Each table entry is a Linux KEY_* code possibly OR'd with modifier flags.
// -1 means "not mappable by direct key events" (caller falls back to clipboard).
//
// To add a keyboard layout:
//   1. In keymap.c, write a static const struct keymap_override array listing
//      only the ASCII chars that differ from the US base table (see
//      danish_overrides for the format).
//   2. Add a { "name", overrides, ARRAY_LEN(overrides) } entry to layouts[].
//   3. The full table is built automatically as base us_map + overrides.
// No changes here or in the caller are needed.

#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

#define FLAG_UPPERCASE 0x80000000 // press with Shift
#define FLAG_ALTGR     0x40000000 // press with RightAlt (AltGr)
#define FLAG_DEADKEY   0x20000000 // dead key: follow with Space

#ifndef KEY_102ND
#define KEY_102ND 86 // ISO key left of Z (< > \ on Danish)
#endif

// Returns keycode | FLAG_* modifiers for the given layout, or -1.
int32_t keymap_lookup(const char *layout, unsigned char c);

#endif // KEYMAP_H
