#ifndef GBA_SCENE_H
#define GBA_SCENE_H

#include <stdbool.h>
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

typedef struct gba_metasprite_tile_t {
  int8_t x;
  int8_t y;
  uint16_t tile_index;
  uint8_t palette_bank;
  bool hflip;
  bool vflip;
} gba_metasprite_tile_t;

// An inclusive range of frame indices in a sprite's frames table.
typedef struct gba_sprite_anim_t {
  uint8_t start;
  uint8_t end;
} gba_sprite_anim_t;

typedef struct gba_sprite_def_t {
  uint16_t tileset_len;
  const uint8_t *tileset;
  uint16_t tile_count;
  // Default/fallback frame. This remains valid for single-frame sprites and
  // callers that do not provide the optional animation metadata below.
  uint8_t metasprite_len;
  const gba_metasprite_tile_t *metasprite;
  // Optional animation metadata. Zero/NULL values preserve the legacy
  // single-frame behavior until the renderer opts into animated frames.
  uint8_t frame_count;
  const gba_metasprite_tile_t *const *frames;
  const uint8_t *frame_lengths;
  uint8_t anim_count;
  const gba_sprite_anim_t *animations;
} gba_sprite_def_t;

typedef struct gba_actor_def_t {
  uint16_t x;
  uint16_t y;
  uint8_t sprite_index;
  uint8_t direction;
  uint8_t move_speed;
  uint8_t anim_speed;
  bool collision_enabled;
  bool persistent;
  bool pinned;
  bool hidden;
  const uint8_t *interact_script;
} gba_actor_def_t;

typedef struct gba_scene_def_t {
  uint8_t width;
  uint8_t height;
  uint8_t type;
  uint8_t player_sprite_index;
  uint8_t actor_count;
  uint8_t trigger_count;
  uint16_t tileset_len;
  const uint8_t *tileset;
  const uint8_t *tilemap;
  const uint8_t *tilemap_attr;
  const uint16_t *bg_palette;
  const uint16_t *sprite_palette;
  const uint8_t *collisions;
  const gba_actor_def_t *actors;
  uint8_t sprite_count;
  const gba_sprite_def_t *const *sprites;
  const gba_trigger_def_t *triggers;
  // Optional scene-start script. Scheduled after the player and scene actors
  // have spawned so actor opcodes can safely target their runtime indices.
  const uint8_t *start_script;
} gba_scene_def_t;

// Scene type identifiers (must match compileData.ts sceneTypeIds)
#define SCENE_TYPE_TOPDOWN    0
#define SCENE_TYPE_PLATFORM   1
#define SCENE_TYPE_ADVENTURE  2
#define SCENE_TYPE_SHMUP      3
#define SCENE_TYPE_POINTNCLICK 4
#define SCENE_TYPE_LOGO       5
#define SCENE_TYPE_ISOMETRIC  6

// Extended scene definition for isometric scenes.
// Safe to cast gba_iso_scene_def_t* <-> gba_scene_def_t* because base is first.
typedef struct gba_iso_scene_def_t {
  gba_scene_def_t base;
  uint8_t iso_tile_w;  // projected tile width  in screen px (default 32)
  uint8_t iso_tile_h;  // projected tile height in screen px (default 16)
} gba_iso_scene_def_t;

typedef struct gba_game_data_t {
  uint8_t scene_count;
  uint8_t start_scene_index;
  uint8_t start_x;
  uint8_t start_y;
  uint8_t start_direction;
  uint8_t start_move_speed;
  uint8_t start_anim_speed;
  const gba_scene_def_t *const *scenes;
  const uint8_t *bootstrap_script;
} gba_game_data_t;

#endif
