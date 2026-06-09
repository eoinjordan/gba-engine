#ifndef TEST_GBA_SCENE_DATA_H
#define TEST_GBA_SCENE_DATA_H

#include "gba_scene.h"

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
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
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
    6,
    6,
    0,
    0,
    0,
    2,
    sizeof(scene0_tileset),
    scene0_tileset,
    scene0_tilemap,
    scene0_collisions,
    NULL,
    NULL,
};

static const gba_scene_def_t test_scene1 = {
    4,
    4,
    0,
    0,
    0,
    3,
    sizeof(scene1_tileset),
    scene1_tileset,
    scene1_tilemap,
    scene1_collisions,
    NULL,
    NULL,
};

// A scene with a single trigger zone in tile coordinates (2,2)-(4,4), i.e.
// pixels (16,16)-(32,32) at TILE_WIDTH/HEIGHT == 8 — used to exercise
// engine_update's trigger-overlap wiring (see test_engine_integration.c).
//
// The script sets the scene's palette tone to 2 (VM_OP_SET_SCENE_TONE) — a
// distinctly observable, already-test-covered side effect (see
// vm_scene_set_tone_reloads_the_palette_for_the_current_scene) that proves
// the trigger's script actually ran rather than merely being scheduled.
static const uint8_t test_trigger_script[] = {
    0x02, 0x02,
    0x00,
};

static const gba_trigger_def_t test_scene2_triggers[] = {
    {2, 2, 2, 2, test_trigger_script},
};

static const gba_scene_def_t test_scene2 = {
    6,
    6,
    0,
    0,
    1,
    0,
    sizeof(scene0_tileset),
    scene0_tileset,
    scene0_tilemap,
    scene0_collisions,
    NULL,
    test_scene2_triggers,
};

static const gba_scene_def_t *const test_scenes[] = {
    &test_scene0,
    &test_scene1,
    &test_scene2,
};

static const uint8_t test_bootstrap_script[] = {
    0x01, 0x00, 0x00,
};

static const gba_game_data_t gba_game_data = {
    3,
    0,
    0,
    0,
    test_scenes,
    test_bootstrap_script,
};

#endif
