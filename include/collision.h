#ifndef GBA_COLLISION_H
#define GBA_COLLISION_H

// ---------------------------------------------------------------------------
// Tile-based collision
//
// GB Studio scenes carry a per-tile collision map (one byte per tile; a
// non-zero value means "solid"). Actors are axis-aligned pixel-space
// rectangles. This module is the pure-math layer that answers "does this
// rectangle overlap a solid tile?" and "how far can this actor move before
// it would?" — the foundation of the "actor walks around a scene and bumps
// into things" loop every GB Studio scene type relies on.
//
// Deliberately hardware-free: no VRAM/OAM access, just array lookups and
// integer math, so it's fully host-testable like vm.c and camera.c.
// ---------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

// Returns true if the pixel-space rectangle
// [left_px, left_px + width_px) x [top_px, top_px + height_px) overlaps any
// tile in `collisions` whose value is non-zero ("solid").
//
// `collisions` is a row-major array of `map_width_tiles * map_height_tiles`
// bytes, one per TILE_WIDTH x TILE_HEIGHT tile (matches gba_scene_def_t /
// scene_t's collision map layout). Tiles outside the map bounds are treated
// as solid — actors can't walk off the edge of the world.
//
// A NULL `collisions` pointer means "no collision data" — always returns
// false (nothing is solid), matching the engine's current placeholder-scene
// behaviour.
bool collision_rect_overlaps_solid(const uint8_t *collisions,
                                    uint8_t map_width_tiles,
                                    uint8_t map_height_tiles, int16_t left_px,
                                    int16_t top_px, uint16_t width_px,
                                    uint16_t height_px);

// Resolves how far an actor's bounding box can move toward
// (left_px + dx, top_px + dy) before it would overlap a solid tile, moving
// each axis independently (the standard "slide along walls" technique:
// resolve X first, then Y from the X-resolved position, so an actor sliding
// diagonally into a wall keeps moving along it rather than stopping dead).
//
// Writes the actually-achievable delta to `*out_dx`/`*out_dy` (each is the
// original delta, or 0 if even a single pixel of movement on that axis would
// collide). Neither output pointer may be NULL.
void collision_resolve_movement(const uint8_t *collisions,
                                 uint8_t map_width_tiles,
                                 uint8_t map_height_tiles, int16_t left_px,
                                 int16_t top_px, uint16_t width_px,
                                 uint16_t height_px, int16_t dx, int16_t dy,
                                 int16_t *out_dx, int16_t *out_dy);

#endif
