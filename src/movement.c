#include "movement.h"
#include <stddef.h>

static int16_t clamp_step(int32_t delta, int16_t speed) {
  if (delta > speed) {
    return speed;
  }
  if (delta < -(int32_t)speed) {
    return (int16_t)(-(int32_t)speed);
  }
  return (int16_t)delta;
}

static int32_t abs32(int32_t value) { return value < 0 ? -value : value; }

void movement_patrol(int16_t actor_x, int16_t actor_y, int16_t bounds_x,
                      int16_t bounds_y, uint16_t bounds_w, uint16_t bounds_h,
                      int16_t speed, bool *inout_moving_positive,
                      int16_t *out_vel_x, int16_t *out_vel_y) {
  if (out_vel_x == NULL || out_vel_y == NULL) {
    return;
  }

  *out_vel_x = 0;
  *out_vel_y = 0;

  if (inout_moving_positive == NULL || (bounds_w == 0 && bounds_h == 0)) {
    return;
  }

  bool horizontal = bounds_w >= bounds_h;
  bool moving_positive = *inout_moving_positive;

  if (horizontal) {
    int32_t min_edge = bounds_x;
    int32_t max_edge = (int32_t)bounds_x + (int32_t)bounds_w;

    if (moving_positive) {
      if ((int32_t)actor_x >= max_edge) {
        moving_positive = false;
      }
    } else {
      if ((int32_t)actor_x <= min_edge) {
        moving_positive = true;
      }
    }

    *out_vel_x = moving_positive ? speed : (int16_t)(-(int32_t)speed);
  } else {
    int32_t min_edge = bounds_y;
    int32_t max_edge = (int32_t)bounds_y + (int32_t)bounds_h;

    if (moving_positive) {
      if ((int32_t)actor_y >= max_edge) {
        moving_positive = false;
      }
    } else {
      if ((int32_t)actor_y <= min_edge) {
        moving_positive = true;
      }
    }

    *out_vel_y = moving_positive ? speed : (int16_t)(-(int32_t)speed);
  }

  *inout_moving_positive = moving_positive;
}

void movement_follow(int16_t actor_x, int16_t actor_y, int16_t target_x,
                      int16_t target_y, int16_t speed, uint16_t range,
                      int16_t *out_vel_x, int16_t *out_vel_y) {
  if (out_vel_x == NULL || out_vel_y == NULL) {
    return;
  }

  int32_t dx = (int32_t)target_x - (int32_t)actor_x;
  int32_t dy = (int32_t)target_y - (int32_t)actor_y;

  if (abs32(dx) > (int32_t)range || abs32(dy) > (int32_t)range) {
    *out_vel_x = 0;
    *out_vel_y = 0;
    return;
  }

  *out_vel_x = clamp_step(dx, speed);
  *out_vel_y = clamp_step(dy, speed);
}
