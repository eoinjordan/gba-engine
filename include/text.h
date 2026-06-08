#ifndef GBA_TEXT_H
#define GBA_TEXT_H

// ---------------------------------------------------------------------------
// Dialogue text helpers
//
// GB Studio dialogue boxes do two things before a single glyph hits the
// screen: substitute `{variable}`-style placeholders with live game state,
// and word-wrap the result to fit a fixed-width text box. Both are pure
// string/integer logic with no hardware dependency — the actual glyph
// rendering (fonts, VRAM tile writes) is a separate, hardware-facing concern
// this module deliberately knows nothing about, so the logic that's easiest
// to get subtly wrong is the easiest to test.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include <stdint.h>

// Expands `{N}` placeholders in `template_str` (where N is a decimal VM
// variable index, 0-255) with the current decimal value of `vm_variables[N]`
// (see vm.h), writing a NUL-terminated result into `out`/`out_size`.
//
// Malformed or out-of-range placeholders ("{", "{abc}", "{999}", an
// unterminated "{12") are copied through verbatim rather than substituted —
// a typo in a dialogue script should never crash the VM or corrupt output.
//
// The output is always NUL-terminated (when out_size > 0) and truncated to
// fit if necessary. Returns the number of characters written, excluding the
// NUL terminator (i.e. like snprintf's truncation semantics, but capped at
// out_size - 1 rather than reporting the untruncated length).
size_t text_format_variables(const char *template_str, char *out,
                             size_t out_size);

// Greedily word-wraps `text` so that no line exceeds `max_chars_per_line`
// characters, writing the result (with '\n' inserted at break points) into
// a NUL-terminated `out`/`out_size`.
//
// Rules:
//   - Existing whitespace runs are collapsed to a single space when they
//     fall mid-line, and dropped entirely at a line break (no trailing
//     spaces before a wrap).
//   - Existing '\n' in `text` are preserved as hard breaks and reset the
//     line-length counter.
//   - A single word longer than `max_chars_per_line` is hard-broken at the
///    limit rather than overflowing the line or being dropped.
//   - `max_chars_per_line == 0` disables wrapping (only hard '\n' breaks
//     apply) — useful for callers that size text boxes dynamically.
//
// Returns the number of characters written, excluding the NUL terminator
// (truncated to fit `out_size`, like text_format_variables).
size_t text_word_wrap(const char *text, uint8_t max_chars_per_line, char *out,
                      size_t out_size);

#endif
