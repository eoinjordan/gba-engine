#ifndef GBA_SAVEGAME_H
#define GBA_SAVEGAME_H

// ---------------------------------------------------------------------------
// Save data encode/decode
//
// SRAM access itself is hardware-facing (a battery-backed memory window the
// cart maps in) and can't be exercised on the host. But the *format* —
// turning game state into a byte buffer and safely turning it back — is
// pure, portable logic, and it's also exactly the part most likely to bite
// a player if it's wrong (corrupting their save, or crashing on load).
//
// This module owns that format: a fixed-size, versioned, checksummed record
// that round-trips scene index, player position, and every VM variable.
// Decoding is defensive by design — bad magic, version, or checksum (an
// empty/uninitialised/corrupted SRAM chip, or a save from an incompatible
// build) is reported as failure rather than handed to the caller as if it
// were valid data.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include "vm.h"
#include <stdbool.h>
#include <stdint.h>

// The exact on-"disk" (SRAM) size of an encoded save record. Exposed so
// callers can size buffers/SRAM regions correctly — and so a change to the
// format that affects this size is impossible to make silently.
#define SAVE_RECORD_SIZE (4 + 1 + 1 + 2 + 2 + (VM_VARIABLE_COUNT * 2) + 2)

typedef struct save_state_t {
  uint8_t scene_index;
  uint16_t player_x;
  uint16_t player_y;
  int16_t variables[VM_VARIABLE_COUNT];
} save_state_t;

// Encodes `state` into `out` as a SAVE_RECORD_SIZE-byte record (magic,
// format version, fields, then an additive checksum over everything before
// it). `out_size` must be at least SAVE_RECORD_SIZE; on success returns
// SAVE_RECORD_SIZE, on a too-small buffer returns 0 and writes nothing.
size_t save_encode(const save_state_t *state, uint8_t *out, size_t out_size);

// Decodes a record produced by save_encode from `in` (at least
// SAVE_RECORD_SIZE bytes) into `*out_state`.
//
// Returns false — and leaves `*out_state` untouched — if `in_size` is too
// small, the magic/version don't match what this build writes, or the
// checksum doesn't match (corrupted or foreign data). Returns true and
// populates `*out_state` only when the record is fully validated.
bool save_decode(const uint8_t *in, size_t in_size, save_state_t *out_state);

#endif
