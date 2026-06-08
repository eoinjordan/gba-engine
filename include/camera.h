#ifndef GBA_CAMERA_H
#define GBA_CAMERA_H

// ---------------------------------------------------------------------------
// Camera / scroll
//
// GB Studio scenes are commonly larger than a single screen; the engine
// scrolls the background to follow the player. This module is the pure-math
// half of that: given a target position (typically the player actor) and the
// pixel dimensions of the current scene, it computes the top-left scroll
// offset that keeps the target centred on screen without ever scrolling past
// the scene's edges.
//
// Deliberately hardware-free (no REG_BG0HOFS/VOFS writes here) so the
// follow/clamp math can be unit tested on the host — the engine is
// responsible for taking `camera.x`/`camera.y` and writing them to the
// scroll registers each frame.
// ---------------------------------------------------------------------------

#include <stdint.h>

typedef struct camera_t {
  int16_t x;
  int16_t y;
} camera_t;

// Recomputes `camera`'s scroll position to keep (target_x, target_y) centred
// in a `viewport_w` x `viewport_h` viewport, clamped so the viewport never
// shows past the edges of a `scene_width_px` x `scene_height_px` world.
//
// If the world is no larger than the viewport on a given axis, that axis is
// locked to 0 — there's nothing to scroll, so the renderer is expected to
// show the (centred/letterboxed) world as-is.
//
// `camera` may not be NULL.
void camera_follow(camera_t *camera, int16_t target_x, int16_t target_y,
                   uint16_t viewport_w, uint16_t viewport_h,
                   uint16_t scene_width_px, uint16_t scene_height_px);

#endif
