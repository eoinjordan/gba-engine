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

static const gba_scene_def_t *const test_scenes[] = {
    &test_scene0,
    &test_scene1,
    &test_scene2,
};

static const uint8_t test_bootstrap_script[] = {
    0x01, 0x00, // VM_OP_LOAD_SCENE 0
    0x00,       // VM_OP_END
};

static const gba_game_data_t gba_game_data = {
    .scene_count       = 3,
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
