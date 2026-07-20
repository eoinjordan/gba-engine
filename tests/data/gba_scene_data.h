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

static const uint8_t scene1_start_script[] = {
    0x02, 0x03, // VM_OP_SET_SCENE_TONE 3
    0x00,       // VM_OP_END
};

static const uint8_t scene4_actor_interact_script[] = {
    VM_OP_SET_SCENE_TONE, 2,
    VM_OP_END,
};

static const gba_actor_def_t scene4_actors[] = {
    {
        .x = 8,
        .y = 0,
        .move_speed = 1,
        .collision_enabled = true,
        .interact_script = scene4_actor_interact_script,
    },
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
    .start_script = scene1_start_script,
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

// Dedicated interaction scene: an actor stands one tile to the right of the
// player and changes the palette when interacted with.
static const gba_scene_def_t test_scene4 = {
    .width         = 4,
    .height        = 4,
    .type          = 0,
    .collisions    = scene1_collisions,
    .actor_count   = 1,
    .actors        = scene4_actors,
};

// Isometric integration scene. Its logical 8x7 collision grid is deliberately
// different from its 30x20 (240x160) compiled background so the runtime must
// honour the two independent coordinate spaces.
static const uint8_t iso_scene_collisions[8 * 7] = {
    [3 * 8 + 4] = 1,
};

static const uint8_t iso_scene_tilemap[30 * 20] = {
    [0] = 1,
    [29] = 2,
    [30] = 3,
    [30 * 19 + 29] = 1,
};

static const uint8_t iso_near_interact_script[] = {
    VM_OP_SET_SCENE_TONE, 1,
    VM_OP_END,
};

static const uint8_t iso_far_interact_script[] = {
    VM_OP_SET_SCENE_TONE, 2,
    VM_OP_END,
};

static const uint8_t iso_trigger_script[] = {
    VM_OP_SET_SCENE_TONE, 3,
    VM_OP_END,
};

static const uint8_t iso_transition_script[] = {
    VM_OP_LOAD_SCENE_AT, 3, 2, 3, 2,
    VM_OP_END,
};

// A 16x16 frame made from two 8x16 objects. Positions are compiler-format
// deltas: +8 followed by -8 accumulates to absolute x positions 8 then 0.
static const uint8_t iso_tall_sprite_tileset[4 * 32] = {0};
static const gba_metasprite_tile_t iso_tall_metasprite[] = {
    {8, 0, 0, 0, false, false},
    {-8, 0, 2, 0, false, false},
};
static const gba_sprite_def_t iso_tall_sprite = {
    .tileset_len    = sizeof(iso_tall_sprite_tileset),
    .tileset        = iso_tall_sprite_tileset,
    .tile_count     = 4,
    .metasprite_len = 2,
    .metasprite     = iso_tall_metasprite,
    .obj_8x16       = true,
};

static const gba_actor_def_t iso_scene_actors[] = {
    {
        .x = 5,
        .y = 5,
        .sprite_index = 0,
        .direction = 1,
        .move_speed = 1,
        .collision_enabled = true,
        .interact_script = iso_near_interact_script,
    },
    {
        .x = 6,
        .y = 5,
        .sprite_index = 0,
        .direction = 2,
        .move_speed = 1,
        .collision_enabled = true,
        .interact_script = iso_far_interact_script,
    },
    {
        .x = 3,
        .y = 3,
        .sprite_index = 1,
        .direction = 0,
        .move_speed = 1,
        .collision_enabled = true,
    },
    {
        .x = 2,
        .y = 2,
        .sprite_index = 0,
        .direction = 0,
        .move_speed = 1,
        .collision_enabled = true,
        .iso_z = 1,
    },
};

static const gba_trigger_def_t iso_scene_triggers[] = {
    {.x = 3, .y = 2, .w = 1, .h = 1, .script = iso_trigger_script},
    {.x = 7, .y = 6, .w = 1, .h = 1, .script = iso_transition_script},
};

static const gba_sprite_def_t *const iso_scene_sprites[] = {
    &scene3_sprite,
    &iso_tall_sprite,
};

static const gba_iso_scene_def_t test_scene5 = {
    .base = {
        .width               = 8,
        .height              = 7,
        .type                = SCENE_TYPE_ISOMETRIC,
        .player_sprite_index = 0,
        .actor_count         = 4,
        .trigger_count       = 2,
        .tileset_len         = sizeof(scene0_tileset),
        .tileset             = scene0_tileset,
        .tilemap             = iso_scene_tilemap,
        .collisions          = iso_scene_collisions,
        .actors              = iso_scene_actors,
        .sprite_count        = 2,
        .sprites             = iso_scene_sprites,
        .triggers            = iso_scene_triggers,
        .background_width    = 30,
        .background_height   = 20,
    },
    .iso_tile_w = 32,
    .iso_tile_h = 16,
};

// A projected world one hardware tile wider than the viewport, used to prove
// that camera scroll and actor projection remain in the same coordinate space.
static const uint8_t iso_large_collisions[8 * 8] = {0};
static const uint8_t iso_large_tilemap[32 * 20] = {0};
static const gba_iso_scene_def_t test_scene6 = {
    .base = {
        .width               = 8,
        .height              = 8,
        .type                = SCENE_TYPE_ISOMETRIC,
        .player_sprite_index = 0,
        .tileset_len         = sizeof(scene0_tileset),
        .tileset             = scene0_tileset,
        .tilemap             = iso_large_tilemap,
        .collisions          = iso_large_collisions,
        .sprite_count        = 1,
        .sprites             = scene3_sprites,
        .background_width    = 32,
        .background_height   = 20,
    },
    .iso_tile_w = 32,
    .iso_tile_h = 16,
};

static const gba_scene_def_t *const test_scenes[] = {
    &test_scene0,
    &test_scene1,
    &test_scene2,
    &test_scene3,
    &test_scene4,
    (const gba_scene_def_t *)&test_scene5,
    (const gba_scene_def_t *)&test_scene6,
};

static const uint8_t test_bootstrap_script[] = {
    0x01, 0x00, // VM_OP_LOAD_SCENE 0
    0x00,       // VM_OP_END
};

static const gba_game_data_t gba_game_data = {
    .scene_count       = 7,
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
