#include "text.h"
#include "vm.h"
#include <stdbool.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Small output-buffer helpers
//
// All writes go through these so truncation is handled in exactly one place:
// never write past `out_size - 1`, always leave room for the NUL terminator,
// and let the running position double as the "characters written" count.
// ---------------------------------------------------------------------------

static size_t append_char(char *out, size_t out_size, size_t pos, char c) {
  if (pos + 1 < out_size) {
    out[pos] = c;
    return pos + 1;
  }
  return pos;
}

static size_t append_str(char *out, size_t out_size, size_t pos,
                          const char *s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    pos = append_char(out, out_size, pos, s[i]);
  }
  return pos;
}

// Decimal-formats a signed 16-bit value into `buf` (NUL-terminated), e.g.
// for turning a VM variable's value into dialogue text. Avoids pulling in
// snprintf — this is the entire extent of the formatting this engine needs,
// and keeping it dependency-free keeps the ROM small.
static size_t format_int16(int16_t value, char *buf, size_t buf_size) {
  char digits[6]; // "32768" is the longest magnitude (5 digits)
  size_t digit_count = 0;
  bool negative = value < 0;

  // Negate via a wider type so INT16_MIN (-32768) doesn't overflow.
  uint16_t magnitude =
      negative ? (uint16_t)(-(int32_t)value) : (uint16_t)value;

  do {
    digits[digit_count++] = (char)('0' + (magnitude % 10));
    magnitude /= 10;
  } while (magnitude != 0);

  size_t len = 0;
  if (negative && len + 1 < buf_size) {
    buf[len++] = '-';
  }
  while (digit_count > 0 && len + 1 < buf_size) {
    buf[len++] = digits[--digit_count];
  }
  buf[len] = '\0';
  return len;
}

size_t text_format_variables(const char *template_str, char *out,
                             size_t out_size) {
  if (out == NULL || out_size == 0) {
    return 0;
  }
  if (template_str == NULL) {
    out[0] = '\0';
    return 0;
  }

  size_t pos = 0;
  const char *p = template_str;

  while (*p != '\0') {
    if (*p == '{') {
      const char *digits_start = p + 1;
      const char *q = digits_start;
      uint32_t value = 0;

      while (*q >= '0' && *q <= '9') {
        // Cap the accumulator so long digit runs ("{999999999999}") can't
        // overflow — anything this large is out of range anyway.
        if (value <= 0xFFFFu) {
          value = value * 10u + (uint32_t)(*q - '0');
        }
        q++;
      }

      bool has_digits = q > digits_start;
      bool well_formed = has_digits && *q == '}';

      if (well_formed && value < VM_VARIABLE_COUNT) {
        char num_buf[8];
        size_t num_len = format_int16(vm_variables[value], num_buf,
                                       sizeof(num_buf));
        pos = append_str(out, out_size, pos, num_buf, num_len);
        p = q + 1; // consume the trailing '}'
        continue;
      }

      // Not a valid/in-range placeholder — emit '{' literally and resume
      // scanning from the very next character (so e.g. "{0}" inside a
      // malformed run is still recognised on a later pass through the loop).
      pos = append_char(out, out_size, pos, '{');
      p++;
      continue;
    }

    pos = append_char(out, out_size, pos, *p);
    p++;
  }

  out[pos] = '\0';
  return pos;
}

size_t text_word_wrap(const char *text, uint8_t max_chars_per_line, char *out,
                      size_t out_size) {
  if (out == NULL || out_size == 0) {
    return 0;
  }
  if (text == NULL) {
    out[0] = '\0';
    return 0;
  }

  size_t pos = 0;
  uint8_t line_len = 0;
  bool at_line_start = true;
  const char *p = text;

  while (*p != '\0') {
    // Spaces/tabs are separators — collapsed to a single space and only
    // emitted lazily, once we know whether the next word fits.
    if (*p == ' ' || *p == '\t') {
      p++;
      continue;
    }

    if (*p == '\n') {
      pos = append_char(out, out_size, pos, '\n');
      line_len = 0;
      at_line_start = true;
      p++;
      continue;
    }

    const char *word_start = p;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
      p++;
    }
    size_t word_len = (size_t)(p - word_start);

    if (max_chars_per_line == 0) {
      // Wrapping disabled: just join words with single spaces and let
      // explicit '\n's (handled above) be the only breaks.
      if (!at_line_start) {
        pos = append_char(out, out_size, pos, ' ');
      }
      pos = append_str(out, out_size, pos, word_start, word_len);
      at_line_start = false;
      continue;
    }

    if (!at_line_start) {
      size_t needed = (size_t)line_len + 1 + word_len;
      if (needed > max_chars_per_line) {
        // Doesn't fit even with a separating space — wrap before it.
        pos = append_char(out, out_size, pos, '\n');
        line_len = 0;
        at_line_start = true;
      } else {
        pos = append_char(out, out_size, pos, ' ');
        line_len = (uint8_t)(line_len + 1);
      }
    }

    // Emit the word, hard-breaking mid-word if it alone exceeds a line —
    // never silently overflow or drop characters.
    for (size_t i = 0; i < word_len; i++) {
      if (line_len >= max_chars_per_line) {
        pos = append_char(out, out_size, pos, '\n');
        line_len = 0;
      }
      pos = append_char(out, out_size, pos, word_start[i]);
      line_len = (uint8_t)(line_len + 1);
    }
    at_line_start = false;
  }

  out[pos] = '\0';
  return pos;
}
