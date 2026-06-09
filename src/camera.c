#include "camera.h"
#include <stddef.h>

static int16_t clamp_axis(int16_t desired, uint16_t viewport,
                           uint16_t world_px) {
  // World fits entirely within the viewport on this axis — nothing to
  // scroll. Lock to 0 so the renderer can show the whole thing un-scrolled.
  if (world_px <= viewport) {
    return 0;
  }

  int16_t max_scroll = (int16_t)(world_px - viewport);

  if (desired < 0) {
    return 0;
  }
  if (desired > max_scroll) {
    return max_scroll;
  }
  return desired;
}

void camera_follow(camera_t *camera, int16_t target_x, int16_t target_y,
                    uint16_t viewport_w, uint16_t viewport_h,
                    uint16_t scene_width_px, uint16_t scene_height_px) {
  if (camera == NULL) {
    return;
  }

  int16_t desired_x = (int16_t)(target_x - (int16_t)(viewport_w / 2));
  int16_t desired_y = (int16_t)(target_y - (int16_t)(viewport_h / 2));

  camera->x = clamp_axis(desired_x, viewport_w, scene_width_px);
  camera->y = clamp_axis(desired_y, viewport_h, scene_height_px);
}
