#include "trigger.h"
#include <stddef.h>

bool trigger_rects_overlap(int16_t a_x, int16_t a_y, uint16_t a_w,
                           uint16_t a_h, int16_t b_x, int16_t b_y,
                           uint16_t b_w, uint16_t b_h) {
  if (a_w == 0 || a_h == 0 || b_w == 0 || b_h == 0) {
    return false;
  }

  int32_t a_left = a_x;
  int32_t a_right = (int32_t)a_x + (int32_t)a_w;
  int32_t a_top = a_y;
  int32_t a_bottom = (int32_t)a_y + (int32_t)a_h;

  int32_t b_left = b_x;
  int32_t b_right = (int32_t)b_x + (int32_t)b_w;
  int32_t b_top = b_y;
  int32_t b_bottom = (int32_t)b_y + (int32_t)b_h;

  if (a_right <= b_left || b_right <= a_left) {
    return false;
  }
  if (a_bottom <= b_top || b_bottom <= a_top) {
    return false;
  }

  return true;
}

int trigger_find_overlap(int16_t actor_x, int16_t actor_y, uint16_t actor_w,
                         uint16_t actor_h, const int16_t *trigger_x,
                         const int16_t *trigger_y, const uint16_t *trigger_w,
                         const uint16_t *trigger_h, uint8_t trigger_count) {
  if (trigger_x == NULL || trigger_y == NULL || trigger_w == NULL ||
      trigger_h == NULL) {
    return TRIGGER_NONE;
  }

  for (uint8_t i = 0; i < trigger_count; i++) {
    if (trigger_rects_overlap(actor_x, actor_y, actor_w, actor_h,
                              trigger_x[i], trigger_y[i], trigger_w[i],
                              trigger_h[i])) {
      return (int)i;
    }
  }

  return TRIGGER_NONE;
}
