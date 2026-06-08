#ifndef GBA_SCENE_H
#define GBA_SCENE_H

#include <stdint.h>

// A compiled scene-trigger zone (GB Studio "trigger" convention): a
// rectangular region in *tile* coordinates — mirroring how `collisions` is
// tile-indexed — that runs `script` once when the player actor (actors[0])
// enters it. Re-entering after leaving fires it again, but standing inside
// it does not re-fire every frame (see engine.c's current_trigger_index).
typedef struct gba_trigger_def_t {
  uint8_t x;
  uint8_t y;
  uint8_t w;
  uint8_t h;
  const uint8_t *script;
} gba_trigger_def_t;

typedef struct gba_scene_def_t {
  uint8_t width;
  uint8_t height;
  uint8_t type;
  uint8_t actor_count;
  uint8_t trigger_count;
  uint8_t palette_tone;
  const uint8_t *collisions;
  const uint8_t *init_script;
  const gba_trigger_def_t *triggers;
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
