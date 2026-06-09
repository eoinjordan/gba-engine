#include "savegame.h"
#include <stddef.h>

#define SAVE_MAGIC_0 ((uint8_t)'G')
#define SAVE_MAGIC_1 ((uint8_t)'B')
#define SAVE_MAGIC_2 ((uint8_t)'A')
#define SAVE_MAGIC_3 ((uint8_t)'S')
#define SAVE_FORMAT_VERSION ((uint8_t)1)

// Layout (all multi-byte fields little-endian, written/read explicitly byte
// by byte so the format is identical on the host and on the ARM7TDMI
// regardless of either compiler's native struct layout/endianness):
//
//   offset  size  field
//   0       4     magic "GBAS"
//   4       1     format version
//   5       1     scene_index
//   6       2     player_x
//   8       2     player_y
//   10      2*N   variables[0..VM_VARIABLE_COUNT-1]
//   ...     2     additive checksum over bytes [0, checksum_offset)

#define SAVE_OFFSET_MAGIC 0
#define SAVE_OFFSET_VERSION 4
#define SAVE_OFFSET_SCENE_INDEX 5
#define SAVE_OFFSET_PLAYER_X 6
#define SAVE_OFFSET_PLAYER_Y 8
#define SAVE_OFFSET_VARIABLES 10
#define SAVE_OFFSET_CHECKSUM (SAVE_OFFSET_VARIABLES + (VM_VARIABLE_COUNT * 2))

static void write_u8(uint8_t *out, size_t offset, uint8_t value) {
  out[offset] = value;
}

static void write_u16(uint8_t *out, size_t offset, uint16_t value) {
  out[offset] = (uint8_t)(value & 0xFF);
  out[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
}

static uint8_t read_u8(const uint8_t *in, size_t offset) {
  return in[offset];
}

static uint16_t read_u16(const uint8_t *in, size_t offset) {
  return (uint16_t)((uint16_t)in[offset] | ((uint16_t)in[offset + 1] << 8));
}

// Simple additive checksum (sum of bytes, wrapping at 16 bits). Not
// cryptographic — just enough to catch torn writes, uninitialised SRAM
// (typically all 0x00 or all 0xFF, neither of which produces a matching
// magic+checksum pair), and gross corruption.
static uint16_t checksum(const uint8_t *data, size_t length) {
  uint16_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum = (uint16_t)(sum + data[i]);
  }
  return sum;
}

size_t save_encode(const save_state_t *state, uint8_t *out, size_t out_size) {
  if (state == NULL || out == NULL || out_size < SAVE_RECORD_SIZE) {
    return 0;
  }

  write_u8(out, SAVE_OFFSET_MAGIC + 0, SAVE_MAGIC_0);
  write_u8(out, SAVE_OFFSET_MAGIC + 1, SAVE_MAGIC_1);
  write_u8(out, SAVE_OFFSET_MAGIC + 2, SAVE_MAGIC_2);
  write_u8(out, SAVE_OFFSET_MAGIC + 3, SAVE_MAGIC_3);
  write_u8(out, SAVE_OFFSET_VERSION, SAVE_FORMAT_VERSION);
  write_u8(out, SAVE_OFFSET_SCENE_INDEX, state->scene_index);
  write_u16(out, SAVE_OFFSET_PLAYER_X, state->player_x);
  write_u16(out, SAVE_OFFSET_PLAYER_Y, state->player_y);

  for (size_t i = 0; i < VM_VARIABLE_COUNT; i++) {
    write_u16(out, SAVE_OFFSET_VARIABLES + (i * 2),
              (uint16_t)state->variables[i]);
  }

  write_u16(out, SAVE_OFFSET_CHECKSUM, checksum(out, SAVE_OFFSET_CHECKSUM));

  return SAVE_RECORD_SIZE;
}

bool save_decode(const uint8_t *in, size_t in_size, save_state_t *out_state) {
  if (in == NULL || out_state == NULL || in_size < SAVE_RECORD_SIZE) {
    return false;
  }

  if (read_u8(in, SAVE_OFFSET_MAGIC + 0) != SAVE_MAGIC_0 ||
      read_u8(in, SAVE_OFFSET_MAGIC + 1) != SAVE_MAGIC_1 ||
      read_u8(in, SAVE_OFFSET_MAGIC + 2) != SAVE_MAGIC_2 ||
      read_u8(in, SAVE_OFFSET_MAGIC + 3) != SAVE_MAGIC_3) {
    return false;
  }

  if (read_u8(in, SAVE_OFFSET_VERSION) != SAVE_FORMAT_VERSION) {
    return false;
  }

  uint16_t expected_checksum = read_u16(in, SAVE_OFFSET_CHECKSUM);
  if (checksum(in, SAVE_OFFSET_CHECKSUM) != expected_checksum) {
    return false;
  }

  out_state->scene_index = read_u8(in, SAVE_OFFSET_SCENE_INDEX);
  out_state->player_x = read_u16(in, SAVE_OFFSET_PLAYER_X);
  out_state->player_y = read_u16(in, SAVE_OFFSET_PLAYER_Y);

  for (size_t i = 0; i < VM_VARIABLE_COUNT; i++) {
    out_state->variables[i] =
        (int16_t)read_u16(in, SAVE_OFFSET_VARIABLES + (i * 2));
  }

  return true;
}
