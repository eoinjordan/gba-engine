#include "engine.h"
#include "gba_scene.h"
#include "gba_system.h"
#include "vm.h"
#include <stddef.h>

#if __has_include("data/gba_scene_data.h")
#include "data/gba_scene_data.h"
#define HAS_COMPILED_GBA_GAME_DATA 1
#define GBA_GAME_DATA gba_game_data
#else
#define HAS_COMPILED_GBA_GAME_DATA 0
#define GBA_GAME_DATA fallback_game_data
static const uint8_t fallback_bootstrap_script[] = {
    VM_OP_SET_SCENE_TONE, 0,
    VM_OP_LOAD_SCENE, 0,
    VM_OP_END,
};
static const uint8_t fallback_scene_script[] = {
    VM_OP_SET_SCENE_TONE, 0,
    VM_OP_END,
};
static const uint8_t fallback_collisions[MAP_WIDTH * MAP_HEIGHT] = {0};
static const gba_scene_def_t fallback_scene = {
    MAP_WIDTH,
    MAP_HEIGHT,
    0,
    0,
    0,
    0,
    fallback_collisions,
    fallback_scene_script,
};
static const gba_scene_def_t *const fallback_scenes[] = {&fallback_scene};
static const gba_game_data_t fallback_game_data = {
    1,
    0,
    MAP_WIDTH / 2,
    MAP_HEIGHT / 2,
    fallback_scenes,
    fallback_bootstrap_script,
};
#endif

// ---------------------------------------------------------------------------
// GBA Studio engine - Phase 1
//
// Boots the GBA and renders a single tiled background (Mode 0) the size of one
// GBA screen (30x20 tiles / 240x160px). This is the foundation the VM port
// builds on: the palette / tile / tilemap loading here is the same path real
// compiled scene data will use once the compiler->engine bridge lands.
// ---------------------------------------------------------------------------

// VRAM layout (Mode 0)
#define CHARBLOCK(n) ((volatile uint16_t *)(0x06000000 + (n) * 0x4000))
#define SCREENBLOCK(n) ((volatile uint16_t *)(0x06000000 + (n) * 0x0800))

#define TILE_CHARBLOCK 0
#define MAP_SCREENBLOCK 28

// Map area is a single visible screen
#define SCENE_W MAP_WIDTH  // 30
#define SCENE_H MAP_HEIGHT // 20

// Tile indices in our minimal tileset
#define TILE_BACKDROP 0
#define TILE_FILL 1   // light interior
#define TILE_BORDER 2 // dark edge
#define TILE_CHECKER 3
#define TILE_SOLID 4
#define MAX_ACTORS 16

static scene_t current_scene;
static actor_t actors[MAX_ACTORS];
static const gba_scene_def_t *current_scene_def;
static uint8_t current_scene_index;
static uint8_t current_palette_tone;
static bool engine_running = true;

// 16-colour background palette (bank 0). Mirrors the gba-blank template look:
// dark border + light fill, on the classic GB green ramp.
static const uint16_t bg_palettes[4][8] = {
    {
        RGB15(1, 3, 4),
        RGB15(28, 31, 25),
        RGB15(6, 13, 10),
        RGB15(16, 24, 13),
        RGB15(10, 18, 11),
        0, 0, 0,
    },
    {
        RGB15(2, 5, 8),
        RGB15(16, 24, 31),
        RGB15(4, 8, 18),
        RGB15(8, 14, 24),
        RGB15(22, 28, 31),
        0, 0, 0,
    },
    {
        RGB15(6, 3, 1),
        RGB15(31, 24, 14),
        RGB15(13, 7, 3),
        RGB15(23, 15, 8),
        RGB15(31, 29, 22),
        0, 0, 0,
    },
    {
        RGB15(4, 1, 5),
        RGB15(28, 23, 31),
        RGB15(12, 4, 15),
        RGB15(20, 11, 24),
        RGB15(31, 28, 31),
        0, 0, 0,
    },
};

// One 4bpp tile = 8x8 px, 4 bits/pixel = 32 bytes = 16 uint16 words.
// A solid tile of palette index p has every nibble = p.
static void load_solid_tile(uint16_t tile_index, uint8_t pal_index) {
  volatile uint16_t *tile = CHARBLOCK(TILE_CHARBLOCK) + tile_index * 16;
  uint16_t word = (pal_index << 12) | (pal_index << 8) | (pal_index << 4) |
                  pal_index;
  for (int i = 0; i < 16; i++) {
    tile[i] = word;
  }
}

static void load_checker_tile(uint16_t tile_index, uint8_t a, uint8_t b) {
  volatile uint16_t *tile = CHARBLOCK(TILE_CHARBLOCK) + tile_index * 16;
  uint16_t word_ab = (a << 12) | (b << 8) | (a << 4) | b;
  uint16_t word_ba = (b << 12) | (a << 8) | (b << 4) | a;

  for (int i = 0; i < 16; i++) {
    tile[i] = (i & 1) ? word_ab : word_ba;
  }
}

static void set_map_entry(uint16_t x, uint16_t y, uint16_t tile_index) {
  // Base map is 32x32 tiles regardless of visible area.
  SCREENBLOCK(MAP_SCREENBLOCK)[y * 32 + x] = tile_index;
}

static void render_scene(void) {
  const gba_scene_def_t *scene = current_scene_def;
  uint8_t tone = scene != NULL ? scene->palette_tone : current_palette_tone;
  load_palette(bg_palettes[tone & 0x03], 0, 8);

  load_solid_tile(TILE_BACKDROP, 0);
  load_solid_tile(TILE_FILL, 1);
  load_solid_tile(TILE_BORDER, 2);
  load_checker_tile(TILE_CHECKER, 1, 3);
  load_checker_tile(TILE_SOLID, 2, 4);

  // BG0: charblock 0, screenblock 28, 4bpp, 32x32, priority 0.
  REG_BG0CNT = (TILE_CHARBLOCK << 2) | (MAP_SCREENBLOCK << 8);

  for (uint16_t y = 0; y < 32; y++) {
    for (uint16_t x = 0; x < 32; x++) {
      uint16_t tile = TILE_BACKDROP;
      uint8_t width = scene != NULL ? scene->width : SCENE_W;
      uint8_t height = scene != NULL ? scene->height : SCENE_H;
      uint8_t visible_w = width < SCENE_W ? width : SCENE_W;
      uint8_t visible_h = height < SCENE_H ? height : SCENE_H;

      if (x < visible_w && y < visible_h) {
        bool edge = (x == 0 || y == 0 || x == visible_w - 1 ||
                     y == visible_h - 1);
        tile = edge ? TILE_BORDER : TILE_FILL;

        if (!edge && scene != NULL) {
          uint16_t collision_index = y * width + x;
          if (scene->collisions != NULL && scene->collisions[collision_index]) {
            tile = TILE_SOLID;
          } else if (((x + y + current_scene_index) & 3) == 0) {
            tile = TILE_CHECKER;
          }
        }
      }
      set_map_entry(x, y, tile);
    }
  }
}

static void init_scene_state(void) {
  current_scene.width = MAP_WIDTH;
  current_scene.height = MAP_HEIGHT;
  current_scene.type = 0;
  current_scene.num_actors = 0;
  current_scene.num_triggers = 0;
  current_scene.num_projectiles = 0;
  current_scene.background_index = 0;
  current_scene.palette_index = 0;
  current_scene.actors = actors;
  current_scene_def = NULL;
  current_scene_index = 0;
  current_palette_tone = 0;

  for (uint8_t i = 0; i < MAX_ACTORS; i++) {
    actors[i].active = false;
  }
}

void engine_init(void) {
  gba_init(); // Mode 0, BG0 enabled, VRAM/palette cleared
  init_scene_state();
  script_runner_init(true);
  REG_DISPCNT = MODE_0 | BG0_ENABLE;
  script_execute(0, (UBYTE *)GBA_GAME_DATA.bootstrap_script, NULL, 0);
}

void engine_update(void) {
  // Phase 1: input is read so the edge-detection HAL is exercised; scripted
  // behaviour arrives with the VM port.
  uint16_t keys = get_keys();
  if (keys & KEY_START) {
    load_scene((uint8_t)((current_scene_index + 1) % GBA_GAME_DATA.scene_count));
  }

  (void)script_runner_update();

  for (uint8_t i = 0; i < current_scene.num_actors; i++) {
    actor_t *actor = &actors[i];
    if (!actor->active || actor->disabled) {
      continue;
    }

    actor->x += actor->vel_x;
    actor->y += actor->vel_y;

    if (actor->anim_speed > 0) {
      actor->anim_tick++;
      if (actor->anim_tick >= actor->anim_speed) {
        actor->anim_tick = 0;
        actor->anim_frame++;
      }
    }
  }
}

void engine_render(void) { wait_vblank(); }

void engine_run(void) {
  engine_init();
  while (engine_running) {
    engine_update();
    engine_render();
  }
}

void load_scene(uint8_t scene_index) {
  if (GBA_GAME_DATA.scene_count == 0) {
    return;
  }

  if (scene_index >= GBA_GAME_DATA.scene_count) {
    scene_index = 0;
  }

  current_scene_index = scene_index;
  current_scene_def = GBA_GAME_DATA.scenes[scene_index];

  current_scene.width = current_scene_def->width;
  current_scene.height = current_scene_def->height;
  current_scene.type = current_scene_def->type;
  current_scene.background_index = scene_index;
  current_scene.palette_index = current_scene_def->palette_tone;
  current_scene.num_actors = 0;

  for (uint8_t i = 0; i < MAX_ACTORS; i++) {
    actors[i].active = false;
  }

  render_scene();

  if (current_scene_def->init_script != NULL) {
    script_execute(0, (UBYTE *)current_scene_def->init_script, NULL, 0);
  }
}

void vm_scene_load(uint8_t scene_index) { load_scene(scene_index); }

void vm_scene_set_tone(uint8_t tone) {
  current_palette_tone = tone & 0x03;
  if (current_scene_def != NULL) {
    render_scene();
  }
}

actor_t *spawn_actor(uint8_t sprite_index, uint16_t x, uint16_t y) {
  for (uint8_t i = 0; i < MAX_ACTORS; i++) {
    actor_t *actor = &actors[i];
    if (actor->active) {
      continue;
    }

    actor->active = true;
    actor->pinned = false;
    actor->hidden = false;
    actor->disabled = false;
    actor->anim_noloop = false;
    actor->collision_enabled = true;
    actor->movement_interrupt = false;
    actor->persistent = false;
    actor->sprite_index = sprite_index;
    actor->palette_index = 0;
    actor->x = x;
    actor->y = y;
    actor->vel_x = 0;
    actor->vel_y = 0;
    actor->anim_frame = 0;
    actor->anim_speed = 0;
    actor->anim_tick = 0;
    actor->collision_group = COLLISION_GROUP_NONE;
    actor->movement_type = 0;
    actor->bounds_x = x;
    actor->bounds_y = y;
    actor->bounds_w = TILE_WIDTH;
    actor->bounds_h = TILE_HEIGHT;

    if (i >= current_scene.num_actors) {
      current_scene.num_actors = i + 1;
    }

    return actor;
  }

  return NULL;
}

void destroy_actor(actor_t *actor) {
  if (actor == NULL) {
    return;
  }

  actor->active = false;
  while (current_scene.num_actors > 0 &&
         !actors[current_scene.num_actors - 1].active) {
    current_scene.num_actors--;
  }
}
