#ifndef TEST_GBA_SCENE_DATA_H
#define TEST_GBA_SCENE_DATA_H

#include "gba_scene.h"

// ---------------------------------------------------------------------------
// Test scene data — uses C99 designated initialisers throughout so that
// adding fields to the structs in gba_scene.h never silently initialises new
// members to garbage: missing designators get zero/NULL automatically.
// ---------------------------------------------------------------------------

static const uint8_t scene0_collisions[] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

static const uint8_t scene1_collisions[] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

static const uint8_t scene0_tileset[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
};

static const uint8_t scene0_tilemap[] = {
    1, 2, 3, 1, 2, 3,
    2, 1, 2, 3, 1, 2,
    3, 2, 1, 2, 3, 1,
    1, 3, 2, 1, 2, 3,
    2, 1, 3, 2, 1, 2,
    3, 2, 1, 3, 2, 1,
};

static const uint8_t scene1_tileset[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
};

static const uint8_t scene1_tilemap[] = {
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
};

static const gba_scene_def_t test_scene0 = {
    .width        = 6,
    .height       = 6,
    .type         = 0,
    .tileset_len  = sizeof(scene0_tileset),
    .tileset      = scene0_tileset,
    .tilemap      = scene0_tilemap,
    .collisions   = scene0_collisions,
};

static const gba_scene_def_t test_scene1 = {
    .width        = 4,
    .height       = 4,
    .type         = 0,
    .tileset_len  = sizeof(scene1_tileset),
    .tileset      = scene1_tileset,
    .tilemap      = scene1_tilemap,
    .collisions   = scene1_collisions,
};

// A scene with a single trigger zone in tile coordinates (2,2)-(2×2 tiles),
// i.e. pixels (16,16)-(32,32) at TILE_WIDTH/HEIGHT == 8 — used to exercise
// engine_update's trigger-overlap wiring (see test_engine_integration.c).
//
// The script sets the scene's palette tone to 2 (VM_OP_SET_SCENE_TONE) — a
// distinctly observable, already-test-covered side effect that proves the
// trigger's script actually ran rather than merely being scheduled.
static const uint8_t test_trigger_script[] = {
    0x02, 0x02, // VM_OP_SET_SCENE_TONE 2
    0x00,       // VM_OP_END
};

static const gba_trigger_def_t test_scene2_triggers[] = {
    { .x = 0, .y = 0, .w = 1, .h = 1, .script = test_trigger_script },
};

static const gba_scene_def_t test_scene2 = {
    .width         = 6,
    .height        = 6,
    .type          = 0,
    .trigger_count = 1,
    .tileset_len   = sizeof(scene0_tileset),
    .tileset       = scene0_tileset,
    .tilemap       = scene0_tilemap,
    .collisions    = scene0_collisions,
    .triggers      = test_scene2_triggers,
};

// Eight one-tile frames in compiler animation order:
// down, right, up, left, then the four moving variants.
static const uint8_t scene3_sprite_tileset[8 * 32] = {0};
static const gba_metasprite_tile_t scene3_frame0[] = {{0, 0, 0, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame1[] = {{0, 0, 1, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame2[] = {{0, 0, 2, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame3[] = {{0, 0, 3, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame4[] = {{0, 0, 4, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame5[] = {{0, 0, 5, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame6[] = {{0, 0, 6, 0, false, false}};
static const gba_metasprite_tile_t scene3_frame7[] = {{0, 0, 7, 0, false, false}};
static const gba_metasprite_tile_t *const scene3_frames[] = {
    scene3_frame0, scene3_frame1, scene3_frame2, scene3_frame3,
    scene3_frame4, scene3_frame5, scene3_frame6, scene3_frame7,
};
static const uint8_t scene3_frame_lengths[] = {1, 1, 1, 1, 1, 1, 1, 1};
static const gba_sprite_anim_t scene3_animations[] = {
    {0, 0}, {1, 1}, {2, 2}, {3, 3},
    {4, 4}, {5, 5}, {6, 6}, {7, 7},
};
static const gba_sprite_def_t scene3_sprite = {
    .tileset_len   = sizeof(scene3_sprite_tileset),
    .tileset       = scene3_sprite_tileset,
    .tile_count    = 8,
    .metasprite_len = 1,
    .metasprite    = scene3_frame0,
    .frame_count   = 8,
    .frames        = scene3_frames,
    .frame_lengths = scene3_frame_lengths,
    .anim_count    = 8,
    .animations    = scene3_animations,
};
static const gba_sprite_def_t *const scene3_sprites[] = {&scene3_sprite};
static const gba_scene_def_t test_scene3 = {
    .width               = 4,
    .height              = 4,
    .type                = 0,
    .player_sprite_index = 0,
    .collisions          = scene1_collisions,
    .sprite_count        = 1,
    .sprites             = scene3_sprites,
};

static const gba_scene_def_t *const test_scenes[] = {
    &test_scene0,
    &test_scene1,
    &test_scene2,
    &test_scene3,
};

static const uint8_t test_bootstrap_script[] = {
    0x01, 0x00, // VM_OP_LOAD_SCENE 0
    0x00,       // VM_OP_END
};

static const gba_game_data_t gba_game_data = {
    .scene_count       = 4,
    .start_scene_index = 0,
    .start_x           = 0,
    .start_y           = 0,
    .start_direction   = 0,
    .start_move_speed  = 1,
    .start_anim_speed  = 15,
    .scenes            = test_scenes,
    .bootstrap_script  = test_bootstrap_script,
};

#endif
