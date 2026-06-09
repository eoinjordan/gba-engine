#include "collision.h"
#include <stddef.h>

// Matches TILE_WIDTH/TILE_HEIGHT in gba_types.h (8x8px tiles, Mode 0
// background tiles on the GBA — this is fixed by the hardware, not
// configurable per-project, so it's safe to mirror here rather than pull in
// the hardware-facing headers).
#define COLLISION_TILE_W 8
#define COLLISION_TILE_H 8

static bool tile_is_solid(const uint8_t *collisions, uint8_t map_width_tiles,
                           uint8_t map_height_tiles, int16_t tile_x,
                           int16_t tile_y) {
  // Out-of-bounds tiles are solid: an actor can't walk off the edge of the
  // world even if the scene's collision map doesn't explicitly wall it in.
  if (tile_x < 0 || tile_y < 0 || tile_x >= (int16_t)map_width_tiles ||
      tile_y >= (int16_t)map_height_tiles) {
    return true;
  }

  if (collisions == NULL) {
    return false;
  }

  uint16_t index = (uint16_t)tile_y * map_width_tiles + (uint16_t)tile_x;
  return collisions[index] != 0;
}

bool collision_rect_overlaps_solid(const uint8_t *collisions,
                                    uint8_t map_width_tiles,
                                    uint8_t map_height_tiles, int16_t left_px,
                                    int16_t top_px, uint16_t width_px,
                                    uint16_t height_px) {
  if (width_px == 0 || height_px == 0) {
    return false;
  }

  int16_t right_px = (int16_t)(left_px + (int16_t)width_px - 1);
  int16_t bottom_px = (int16_t)(top_px + (int16_t)height_px - 1);

  int16_t tile_left = (int16_t)(left_px < 0 ? (left_px - (COLLISION_TILE_W - 1)) / COLLISION_TILE_W
                                             : left_px / COLLISION_TILE_W);
  int16_t tile_top = (int16_t)(top_px < 0 ? (top_px - (COLLISION_TILE_H - 1)) / COLLISION_TILE_H
                                           : top_px / COLLISION_TILE_H);
  int16_t tile_right = (int16_t)(right_px < 0 ? (right_px - (COLLISION_TILE_W - 1)) / COLLISION_TILE_W
                                              : right_px / COLLISION_TILE_W);
  int16_t tile_bottom = (int16_t)(bottom_px < 0 ? (bottom_px - (COLLISION_TILE_H - 1)) / COLLISION_TILE_H
                                                : bottom_px / COLLISION_TILE_H);

  for (int16_t ty = tile_top; ty <= tile_bottom; ty++) {
    for (int16_t tx = tile_left; tx <= tile_right; tx++) {
      if (tile_is_solid(collisions, map_width_tiles, map_height_tiles, tx,
                        ty)) {
        return true;
      }
    }
  }

  return false;
}

// Steps a single axis from `from` toward `from + delta` one pixel at a time,
// stopping at the last position that doesn't collide. One-pixel-at-a-time is
// the simplest correct approach for the small per-frame deltas GBA actors
// move at (a handful of pixels), and keeps this trivially easy to verify.
static int16_t resolve_axis(const uint8_t *collisions,
                             uint8_t map_width_tiles, uint8_t map_height_tiles,
                             int16_t fixed_left, int16_t fixed_top,
                             uint16_t width_px, uint16_t height_px,
                             bool moving_x, int16_t delta) {
  if (delta == 0) {
    return 0;
  }

  int16_t step = delta > 0 ? 1 : -1;
  int16_t achieved = 0;
  int16_t remaining = delta;

  while (remaining != 0) {
    int16_t next_left = moving_x ? (int16_t)(fixed_left + achieved + step)
                                 : fixed_left;
    int16_t next_top = moving_x ? fixed_top
                                : (int16_t)(fixed_top + achieved + step);

    if (collision_rect_overlaps_solid(collisions, map_width_tiles,
                                       map_height_tiles, next_left, next_top,
                                       width_px, height_px)) {
      break;
    }

    achieved = (int16_t)(achieved + step);
    remaining = (int16_t)(remaining - step);
  }

  return achieved;
}

void collision_resolve_movement(const uint8_t *collisions,
                                 uint8_t map_width_tiles,
                                 uint8_t map_height_tiles, int16_t left_px,
                                 int16_t top_px, uint16_t width_px,
                                 uint16_t height_px, int16_t dx, int16_t dy,
                                 int16_t *out_dx, int16_t *out_dy) {
  if (out_dx == NULL || out_dy == NULL) {
    return;
  }

  // Resolve X first from the original position...
  int16_t achieved_dx =
      resolve_axis(collisions, map_width_tiles, map_height_tiles, left_px,
                   top_px, width_px, height_px, true, dx);

  // ...then Y from the X-resolved position, so sliding along a wall works.
  int16_t achieved_dy = resolve_axis(
      collisions, map_width_tiles, map_height_tiles,
      (int16_t)(left_px + achieved_dx), top_px, width_px, height_px, false,
      dy);

  *out_dx = achieved_dx;
  *out_dy = achieved_dy;
}
