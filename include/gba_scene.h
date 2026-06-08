#ifndef GBA_SCENE_H
#define GBA_SCENE_H

#include <stdint.h>

typedef struct gba_scene_def_t {
  uint8_t width;
  uint8_t height;
  uint8_t type;
  uint8_t actor_count;
  uint8_t trigger_count;
  uint8_t palette_tone;
  const uint8_t *collisions;
  const uint8_t *init_script;
} gba_scene_def_t;

typedef struct gba_game_data_t {
  uint8_t scene_count;
  uint8_t start_scene_index;
  uint8_t start_x;
  uint8_t start_y;
  const gba_scene_def_t *const *scenes;
  const uint8_t *bootstrap_script;
} gba_game_data_t;

#endif
