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

static const gba_scene_def_t test_scene0 = {
    6,
    6,
    0,
    0,
    0,
    2,
    scene0_collisions,
    NULL,
};

static const gba_scene_def_t test_scene1 = {
    4,
    4,
    0,
    0,
    0,
    3,
    scene1_collisions,
    NULL,
};

static const gba_scene_def_t *const test_scenes[] = {
    &test_scene0,
    &test_scene1,
};

static const uint8_t test_bootstrap_script[] = {
    0x01, 0x00, 0x00,
};

static const gba_game_data_t gba_game_data = {
    2,
    0,
    0,
    0,
    test_scenes,
    test_bootstrap_script,
};

#endif
