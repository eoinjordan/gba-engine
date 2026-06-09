#include "camera.h"
#include "collision.h"
#include "engine.h"
#include "gba_scene.h"
#include "gba_system.h"
#include "movement.h"
#include "textbox.h"
#include "trigger.h"
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
static const uint8_t fallback_collisions[MAP_WIDTH * MAP_HEIGHT] = {0};
// Tileset/tilemap intentionally omitted so render_scene() uses the colourful
// placeholder renderer rather than render_compiled_background() on null data.
static const gba_scene_def_t fallback_scene = {
    .width      = MAP_WIDTH,
    .height     = MAP_HEIGHT,
    .collisions = fallback_collisions,
};
static const gba_scene_def_t *const fallback_scenes[] = {&fallback_scene};
static const gba_game_data_t fallback_game_data = {
    .scene_count       = 1,
    .start_scene_index = 0,
    .start_x           = MAP_WIDTH / 2,
    .start_y           = MAP_HEIGHT / 2,
    .start_move_speed  = 1,
    .start_anim_speed  = 15,
    .scenes            = fallback_scenes,
    .bootstrap_script  = fallback_bootstrap_script,
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
#ifndef GBA_VRAM_ADDR
#define GBA_VRAM_ADDR(offset)                                                 \
  ((volatile uint16_t *)(0x06000000u + (offset)))
#endif

#define CHARBLOCK(n) GBA_VRAM_ADDR((n) * 0x4000u)
#define SCREENBLOCK(n) GBA_VRAM_ADDR((n) * 0x0800u)
#define OBJ_VRAM ((volatile uint16_t *)0x06010000u)

#define TILE_CHARBLOCK 0
#define MAP_SCREENBLOCK 28
#define MAX_SCENE_SPRITES 64
#define MAX_OAM_ENTRIES 128

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
static uint16_t loaded_sprite_tile_bases[MAX_SCENE_SPRITES];
static bool engine_running = true;

// Isometric projection params (only meaningful when scene type == ISOMETRIC).
// iso_origin_x centres the diamond grid: tile(0,0) appears at the top-centre
// of the canvas rather than the top-left corner.
static uint8_t  iso_tile_w   = 32;  // projected tile width  (screen px)
static uint8_t  iso_tile_h   = 16;  // projected tile height (screen px)
static int16_t  iso_origin_x = 0;   // = scene_width * iso_tile_w / 2

// Convert isometric tile coordinates to screen pixel coordinates.
// Actors in ISO scenes store tile indices in x/y; this gives OAM position.
static inline int16_t iso_screen_x(int16_t tile_x, int16_t tile_y) {
  return iso_origin_x + (tile_x - tile_y) * (iso_tile_w / 2);
}
static inline int16_t iso_screen_y(int16_t tile_x, int16_t tile_y) {
  return (tile_x + tile_y) * (iso_tile_h / 2);
}

// Convert screen px back to tile coords (ground plane, iso_z = 0).
static inline int16_t iso_tile_x_from_screen(int16_t sx, int16_t sy) {
  int16_t sx_rel = sx - iso_origin_x;
  return (int16_t)(((int32_t)sy * 2 / iso_tile_h + (int32_t)sx_rel * 2 / iso_tile_w) / 2);
}
static inline int16_t iso_tile_y_from_screen(int16_t sx, int16_t sy) {
  int16_t sx_rel = sx - iso_origin_x;
  return (int16_t)(((int32_t)sy * 2 / iso_tile_h - (int32_t)sx_rel * 2 / iso_tile_w) / 2);
}

// Scroll position. Actor 0 is treated as the player/camera target (the GB
// Studio convention) — when present, the camera centres on it each frame and
// clamps to the scene's pixel bounds so the viewport never shows past an
// edge. Scenes no larger than one screen simply never scroll (camera stays
// at the origin — see camera_follow's "world fits in viewport" case).
static camera_t camera;

// Index into current_scene_def->triggers that the player actor (actors[0])
// is currently standing inside, or TRIGGER_NONE. Persisted across frames so
// a trigger's script fires once on entry rather than every frame the player
// stands in the zone — re-entering after leaving fires it again.
static int current_trigger_index;

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

static void hide_oam_entry(uint16_t index) {
  volatile uint16_t *oam = MEM_OAM + index * 4u;
  oam[0] = 160u;
  oam[1] = 240u;
  oam[2] = 0u;
  oam[3] = 0u;
}

static void hide_all_oam(void) {
  for (uint16_t index = 0; index < MAX_OAM_ENTRIES; index++) {
    hide_oam_entry(index);
  }
}

static void load_scene_obj_palette(const gba_scene_def_t *scene) {
  if (scene == NULL || scene->sprite_palette == NULL) {
    return;
  }

  for (uint16_t bank = 0; bank < 8; bank++) {
    for (uint16_t color = 0; color < 16; color++) {
      MEM_PALETTE[256u + bank * 16u + color] =
          scene->sprite_palette[bank * 16u + color];
    }
  }
}

static void load_scene_bg_palette(const gba_scene_def_t *scene) {
  if (scene == NULL || scene->bg_palette == NULL) {
    return;
  }

  for (uint16_t bank = 0; bank < 8; bank++) {
    for (uint16_t color = 0; color < 16; color++) {
      MEM_PALETTE[bank * 16u + color] = scene->bg_palette[bank * 16u + color];
    }
  }
}

static void load_scene_sprite_tiles(const gba_scene_def_t *scene) {
  uint16_t next_tile_base = 0;

  for (uint16_t i = 0; i < MAX_SCENE_SPRITES; i++) {
    loaded_sprite_tile_bases[i] = 0;
  }

  if (scene == NULL || scene->sprites == NULL) {
    return;
  }

  for (uint16_t sprite_index = 0;
       sprite_index < scene->sprite_count && sprite_index < MAX_SCENE_SPRITES;
       sprite_index++) {
    const gba_sprite_def_t *sprite = scene->sprites[sprite_index];
    volatile uint8_t *obj_vram_bytes = (volatile uint8_t *)OBJ_VRAM;

    if (sprite == NULL || sprite->tileset == NULL || sprite->tileset_len == 0) {
      loaded_sprite_tile_bases[sprite_index] = next_tile_base;
      continue;
    }

    loaded_sprite_tile_bases[sprite_index] = next_tile_base;
    for (uint16_t byte_index = 0; byte_index < sprite->tileset_len; byte_index++) {
      obj_vram_bytes[next_tile_base * 32u + byte_index] = sprite->tileset[byte_index];
    }
    next_tile_base = (uint16_t)(next_tile_base + sprite->tile_count);
  }
}

// Depth-sorted draw order for isometric scenes.
// Actors are sorted by screen_y ascending (lower y = farther from viewer =
// drawn first / behind). For iso: screen_y ∝ tile_x + tile_y, so this is
// equivalent to sorting by depth key without needing to track tile coords
// separately from the actor's x/y fields.
static uint8_t actor_draw_order[MAX_ACTORS];

static void build_iso_draw_order(void) {
  uint8_t count = current_scene.num_actors;
  for (uint8_t i = 0; i < count; i++) {
    actor_draw_order[i] = i;
  }
  // Simple insertion sort — MAX_ACTORS is small (16), so this is fine.
  for (uint8_t i = 1; i < count; i++) {
    uint8_t key = actor_draw_order[i];
    int16_t key_sy = iso_screen_y((int16_t)actors[key].x, (int16_t)actors[key].y);
    int8_t j = (int8_t)i - 1;
    while (j >= 0) {
      uint8_t prev = actor_draw_order[(uint8_t)j];
      int16_t prev_sy = iso_screen_y((int16_t)actors[prev].x, (int16_t)actors[prev].y);
      if (prev_sy <= key_sy) break;
      actor_draw_order[(uint8_t)(j + 1)] = prev;
      j--;
    }
    actor_draw_order[(uint8_t)(j + 1)] = key;
  }
}

static void render_scene_actors(void) {
  uint16_t oam_index = 0;
  bool is_iso = (current_scene.type == SCENE_TYPE_ISOMETRIC);

  hide_all_oam();

  if (current_scene_def == NULL || current_scene_def->sprites == NULL) {
    return;
  }

  // Build draw order: isometric needs depth sort; top-down uses natural order.
  if (is_iso) {
    build_iso_draw_order();
  } else {
    for (uint8_t i = 0; i < current_scene.num_actors; i++) {
      actor_draw_order[i] = i;
    }
  }

  for (uint8_t order_i = 0; order_i < current_scene.num_actors; order_i++) {
    uint8_t actor_index = actor_draw_order[order_i];
    const actor_t *actor = &actors[actor_index];
    if (!actor->active || actor->hidden || actor->disabled) {
      continue;
    }
    if (actor->sprite_index >= current_scene_def->sprite_count) {
      continue;
    }

    const gba_sprite_def_t *sprite = current_scene_def->sprites[actor->sprite_index];
    if (sprite == NULL || sprite->metasprite == NULL) {
      continue;
    }

    // For isometric scenes actor->x/y are tile coords; project to screen.
    // For top-down actor->x/y are already pixel coords.
    int16_t base_sx, base_sy;
    if (is_iso) {
      base_sx = iso_screen_x((int16_t)actor->x, (int16_t)actor->y) - (int16_t)camera.x;
      base_sy = iso_screen_y((int16_t)actor->x, (int16_t)actor->y) - (int16_t)camera.y;
    } else {
      base_sx = (int16_t)actor->x - (int16_t)camera.x;
      base_sy = (int16_t)actor->y - (int16_t)camera.y;
    }

    uint16_t tile_base = loaded_sprite_tile_bases[actor->sprite_index];
    for (uint8_t tile_index = 0; tile_index < sprite->metasprite_len; tile_index++) {
      if (oam_index >= MAX_OAM_ENTRIES) {
        return;
      }

      const gba_metasprite_tile_t *tile = &sprite->metasprite[tile_index];
      int16_t screen_x = base_sx + tile->x;
      int16_t screen_y = base_sy + tile->y;

      if (screen_x <= -8 || screen_x >= SCREEN_WIDTH || screen_y <= -8 ||
          screen_y >= SCREEN_HEIGHT) {
        hide_oam_entry(oam_index++);
        continue;
      }

      volatile uint16_t *oam = MEM_OAM + oam_index * 4u;
      oam[0] = (uint16_t)(screen_y & 0x00FF);
      oam[1] = (uint16_t)(screen_x & 0x01FF);
      if (tile->hflip) {
        oam[1] |= 0x1000u;
      }
      if (tile->vflip) {
        oam[1] |= 0x2000u;
      }
      oam[2] = (uint16_t)((tile_base + tile->tile_index) & 0x03FFu);
      oam[2] |= (uint16_t)((tile->palette_bank & 0x0Fu) << 12);
      oam[3] = 0u;
      oam_index++;
    }
  }
}

static void render_placeholder_scene(const gba_scene_def_t *scene) {
  load_solid_tile(TILE_BACKDROP, 0);
  load_solid_tile(TILE_FILL, 1);
  load_solid_tile(TILE_BORDER, 2);
  load_checker_tile(TILE_CHECKER, 1, 3);
  load_checker_tile(TILE_SOLID, 2, 4);

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

static void render_compiled_background(const gba_scene_def_t *scene) {
  volatile uint16_t *tiles_vram = CHARBLOCK(TILE_CHARBLOCK);
  volatile uint8_t *tiles_vram_bytes = (volatile uint8_t *)tiles_vram;
  uint16_t width = scene->width < 32 ? scene->width : 32;
  uint16_t height = scene->height < 32 ? scene->height : 32;

  for (uint16_t index = 0; index < 0x4000u / sizeof(uint16_t); index++) {
    tiles_vram[index] = 0;
  }

  if (scene->tileset != NULL && scene->tileset_len > 0) {
    for (uint16_t index = 0; index < scene->tileset_len; index++) {
      tiles_vram_bytes[index] = scene->tileset[index];
    }
  }

  for (uint16_t y = 0; y < 32; y++) {
    for (uint16_t x = 0; x < 32; x++) {
      uint16_t entry = TILE_BACKDROP;
      if (scene->tilemap != NULL && x < width && y < height) {
        uint16_t map_index = y * scene->width + x;
        entry = scene->tilemap[map_index];
        if (scene->tilemap_attr != NULL) {
          uint8_t attr = scene->tilemap_attr[map_index];
          entry |= (uint16_t)((attr & 0x07u) << 12);
          if ((attr & 0x20u) != 0) {
            entry |= 0x0400u;
          }
          if ((attr & 0x40u) != 0) {
            entry |= 0x0800u;
          }
        }
      }
      set_map_entry(x, y, entry);
    }
  }
}

static void render_scene(void) {
  const gba_scene_def_t *scene = current_scene_def;
  // current_palette_tone is the single source of truth for which palette
  // bank is active: load_scene seeds it from the scene's compiled default
  // (scene->palette_tone) when entering a scene, and vm_scene_set_tone
  // overrides it at runtime (e.g. a script darkening a room) — render_scene
  // must respect that override rather than reverting to the compiled
  // default on every redraw.
  if (scene != NULL && scene->bg_palette != NULL) {
    load_scene_bg_palette(scene);
  } else {
    uint8_t tone = current_palette_tone;
    load_palette(bg_palettes[tone & 0x03], 0, 8);
  }
  load_scene_obj_palette(scene);
  load_scene_sprite_tiles(scene);

  // BG0: charblock 0, screenblock 28, 4bpp, 32x32, priority 1 (behind BG1 textbox).
  REG_BG0CNT = (TILE_CHARBLOCK << 2) | (MAP_SCREENBLOCK << 8) | 1u;

  if (scene != NULL && scene->tileset != NULL && scene->tilemap != NULL &&
      scene->tileset_len > 0) {
    render_compiled_background(scene);
    return;
  }

  render_placeholder_scene(scene);
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
  camera.x = 0;
  camera.y = 0;
  current_trigger_index = TRIGGER_NONE;
  iso_tile_w = 32;
  iso_tile_h = 16;
  iso_origin_x = 0;

  for (uint8_t i = 0; i < MAX_ACTORS; i++) {
    actors[i].active = false;
  }
}

void engine_init(void) {
  gba_init(); // Mode 0, BG0 enabled, VRAM/palette cleared
  init_scene_state();
  script_runner_init(true);
  hide_all_oam();
  textbox_init();
  // BG0 = game world at priority 1; BG1 = textbox at priority 0 (see
  // textbox.c). With equal priority the lower-indexed BG would win, so BG0
  // must be demoted to priority 1 so BG1 can render on top when visible.
  REG_DISPCNT = MODE_0 | BG0_ENABLE | OBJ_ENABLE | OBJ_1D_MAP;
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

  // Actor interaction: when A is pressed and no textbox or script is blocking,
  // find the nearest non-player actor within one tile (16px) and run its
  // interact_script if it has one.
  if (key_pressed(KEY_A) && current_scene.num_actors > 0 && actors[0].active &&
      !VM_ISLOCKED()) {
    const actor_t *player = &actors[0];
    for (uint8_t ai = 1; ai < current_scene.num_actors; ai++) {
      const actor_t *other = &actors[ai];
      if (!other->active || other->hidden || other->disabled) {
        continue;
      }
      int16_t dx = (int16_t)other->x - (int16_t)player->x;
      int16_t dy = (int16_t)other->y - (int16_t)player->y;
      if (dx < 0) dx = (int16_t)-dx;
      if (dy < 0) dy = (int16_t)-dy;
      // Isometric actors store tile coords; 1 tile proximity means adjacent.
      // Top-down actors store pixel coords; compare against tile pixel size.
      int16_t interact_dist = (current_scene.type == SCENE_TYPE_ISOMETRIC)
                              ? 2 : (int16_t)TILE_WIDTH;
      if (dx <= interact_dist && dy <= interact_dist) {
        // Check if this actor has a compiled interact script.
        if (current_scene_def != NULL &&
            current_scene_def->actors != NULL &&
            ai - 1 < current_scene_def->actor_count) {
          const uint8_t *script =
              current_scene_def->actors[ai - 1].interact_script;
          if (script != NULL) {
            script_execute(0, (UBYTE *)script, NULL, 0);
          }
        }
        break;
      }
    }
  }

  if (current_scene.num_actors > 0 && actors[0].active) {
    actor_t *player = &actors[0];
    int16_t step = (int16_t)(player->move_speed > 0 ? player->move_speed : 1);
    player->vel_x = 0;
    player->vel_y = 0;

    if (current_scene.type == SCENE_TYPE_ISOMETRIC) {
      // Isometric D-pad mapping (tile-grid units):
      //   UP    → NE: tile_y--  → screen_x+, screen_y-
      //   DOWN  → SW: tile_y++  → screen_x-, screen_y+
      //   LEFT  → NW: tile_x--  → screen_x-, screen_y-
      //   RIGHT → SE: tile_x++  → screen_x+, screen_y+
      // vel_x/vel_y store tile delta (1 or -1); collision and position update
      // below keep them in tile coords; projection happens at render time.
      if (keys & KEY_UP)    { player->vel_y = (int16_t)(-step); }
      else if (keys & KEY_DOWN)  { player->vel_y = step; }
      if (keys & KEY_LEFT)  { player->vel_x = (int16_t)(-step); }
      else if (keys & KEY_RIGHT) { player->vel_x = step; }
    } else {
      if (keys & KEY_LEFT) {
        player->vel_x = (int16_t)(-step);
      } else if (keys & KEY_RIGHT) {
        player->vel_x = step;
      }
      if (keys & KEY_UP) {
        player->vel_y = (int16_t)(-step);
      } else if (keys & KEY_DOWN) {
        player->vel_y = step;
      }
    }
  }

  for (uint8_t i = 0; i < current_scene.num_actors; i++) {
    actor_t *actor = &actors[i];
    if (!actor->active || actor->disabled) {
      continue;
    }

    // GB-Studio-style movement patterns: actors[0] is the player (driven by
    // input/scripts, never by a pattern); other actors may be assigned a
    // pattern the engine drives every frame without a script running. These
    // only compute vel_x/vel_y — collision resolution below (shared with
    // player movement) still applies, so patrol/follow actors slide along
    // walls rather than clipping through them.
    if (i > 0) {
      switch (actor->movement_type) {
      case MOVEMENT_TYPE_PATROL: {
        // movement_positive is a single-bit field — its address can't be
        // taken directly, so round-trip through a plain bool local.
        bool moving_positive = actor->movement_positive;
        movement_patrol((int16_t)actor->x, (int16_t)actor->y,
                        (int16_t)actor->movement_bounds_x,
                        (int16_t)actor->movement_bounds_y,
                        actor->movement_bounds_w, actor->movement_bounds_h,
                        (int16_t)actor->move_speed, &moving_positive,
                        &actor->vel_x, &actor->vel_y);
        actor->movement_positive = moving_positive;
        break;
      }
      case MOVEMENT_TYPE_FOLLOW:
        if (current_scene.num_actors > 0 && actors[0].active) {
          movement_follow((int16_t)actor->x, (int16_t)actor->y,
                          (int16_t)actors[0].x, (int16_t)actors[0].y,
                          (int16_t)actor->move_speed,
                          actor->movement_bounds_w, &actor->vel_x,
                          &actor->vel_y);
        } else {
          actor->vel_x = 0;
          actor->vel_y = 0;
        }
        break;
      default:
        break;
      }
    }

    if (actor->vel_x != 0 || actor->vel_y != 0) {
      if (actor->collision_enabled && current_scene_def != NULL &&
          current_scene.type != SCENE_TYPE_ISOMETRIC) {
        // Top-down: pixel-space collision resolution.
        int16_t resolved_dx = 0;
        int16_t resolved_dy = 0;
        collision_resolve_movement(
            current_scene_def->collisions, current_scene.width,
            current_scene.height, (int16_t)actor->x, (int16_t)actor->y,
            actor->bounds_w, actor->bounds_h, actor->vel_x, actor->vel_y,
            &resolved_dx, &resolved_dy);
        actor->x = (uint16_t)((int16_t)actor->x + resolved_dx);
        actor->y = (uint16_t)((int16_t)actor->y + resolved_dy);
      } else if (actor->collision_enabled && current_scene_def != NULL &&
                 current_scene.type == SCENE_TYPE_ISOMETRIC &&
                 current_scene_def->collisions != NULL) {
        // Isometric: actors store tile coords; check each axis independently.
        int16_t nx = (int16_t)actor->x + actor->vel_x;
        int16_t ny = (int16_t)actor->y + actor->vel_y;
        // Clamp to map bounds.
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx >= (int16_t)current_scene.width)  nx = (int16_t)(current_scene.width  - 1);
        if (ny >= (int16_t)current_scene.height) ny = (int16_t)(current_scene.height - 1);
        // Check X axis.
        uint16_t cx = (uint16_t)((int16_t)actor->x + actor->vel_x);
        uint16_t cy = (uint16_t)actor->y;
        if (cx < current_scene.width && cy < current_scene.height &&
            !current_scene_def->collisions[cy * current_scene.width + cx]) {
          actor->x = cx;
        }
        // Check Y axis.
        cx = (uint16_t)actor->x;
        cy = (uint16_t)((int16_t)actor->y + actor->vel_y);
        if (cx < current_scene.width && cy < current_scene.height &&
            !current_scene_def->collisions[cy * current_scene.width + cx]) {
          actor->y = cy;
        }
      } else {
        actor->x = (uint16_t)((int16_t)actor->x + actor->vel_x);
        actor->y = (uint16_t)((int16_t)actor->y + actor->vel_y);
      }
    }

    if (actor->anim_speed > 0) {
      actor->anim_tick++;
      if (actor->anim_tick >= actor->anim_speed) {
        actor->anim_tick = 0;
        actor->anim_frame++;
      }
    }
  }

  // Scene triggers (GB Studio "trigger" convention): zones the player actor
  // (actors[0]) runs a script for once on entry. Compiled trigger bounds are
  // tile-coordinate (mirroring `collisions`), converted to pixel space here
  // to compare against the player's pixel position/size, matching how
  // collision resolution already operates in pixels.
  if (current_scene_def != NULL && current_scene_def->triggers != NULL &&
      current_scene.num_actors > 0 && actors[0].active) {
    const actor_t *player = &actors[0];
    int found = TRIGGER_NONE;

    bool is_iso_triggers = (current_scene.type == SCENE_TYPE_ISOMETRIC);
    for (uint8_t t = 0; t < current_scene_def->trigger_count; t++) {
      const gba_trigger_def_t *trigger = &current_scene_def->triggers[t];
      if (is_iso_triggers) {
        // Isometric: both actor and trigger coords are tile indices — compare
        // directly without scaling by TILE_WIDTH/TILE_HEIGHT.
        if (trigger_rects_overlap(
                (int16_t)player->x, (int16_t)player->y, 1, 1,
                (int16_t)trigger->x, (int16_t)trigger->y,
                (uint16_t)trigger->w, (uint16_t)trigger->h)) {
          found = (int)t;
          break;
        }
      } else {
        if (trigger_rects_overlap(
                (int16_t)player->x, (int16_t)player->y, player->bounds_w,
                player->bounds_h, (int16_t)((uint16_t)trigger->x * TILE_WIDTH),
                (int16_t)((uint16_t)trigger->y * TILE_HEIGHT),
                (uint16_t)trigger->w * TILE_WIDTH,
                (uint16_t)trigger->h * TILE_HEIGHT)) {
          found = (int)t;
          break;
        }
      }
    }

    if (found != current_trigger_index && found != TRIGGER_NONE) {
      const gba_trigger_def_t *trigger = &current_scene_def->triggers[found];
      if (trigger->script != NULL) {
        script_execute(0, (UBYTE *)trigger->script, NULL, 0);
      }
    }

    current_trigger_index = found;
  }

  if (current_scene.num_actors > 0 && actors[0].active) {
    int16_t cam_px, cam_py;
    if (current_scene.type == SCENE_TYPE_ISOMETRIC) {
      // Project tile coords to screen px for camera tracking.
      cam_px = iso_screen_x((int16_t)actors[0].x, (int16_t)actors[0].y);
      cam_py = iso_screen_y((int16_t)actors[0].x, (int16_t)actors[0].y);
      // World pixel extents for an iso scene: the diamond spans
      // scene_width * iso_tile_w wide and (scene_w + scene_h) * iso_tile_h/2 tall.
      uint16_t world_w = (uint16_t)current_scene.width * iso_tile_w;
      uint16_t world_h = (uint16_t)((current_scene.width + current_scene.height)
                                    * (iso_tile_h / 2));
      camera_follow(&camera, cam_px, cam_py, SCREEN_WIDTH, SCREEN_HEIGHT,
                    world_w, world_h);
    } else {
      camera_follow(&camera, (int16_t)actors[0].x, (int16_t)actors[0].y,
                    SCREEN_WIDTH, SCREEN_HEIGHT,
                    (uint16_t)current_scene.width * TILE_WIDTH,
                    (uint16_t)current_scene.height * TILE_HEIGHT);
    }
  } else {
    camera.x = 0;
    camera.y = 0;
  }

  REG_BG0HOFS = (uint16_t)camera.x;
  REG_BG0VOFS = (uint16_t)camera.y;
  render_scene_actors();
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
  current_scene.palette_index = 0;
  // Seed the runtime tone from the scene's compiled default — see
  // render_scene's comment on why current_palette_tone is the source of
  // truth it reads from (this is what makes a freshly-loaded scene render
  // with its own configured tone rather than whatever a previous scene's
  // script last set).
  current_palette_tone = 0;
  current_scene.num_actors = 0;
  current_trigger_index = TRIGGER_NONE;

  for (uint8_t i = 0; i < MAX_ACTORS; i++) {
    actors[i].active = false;
  }

  // For isometric scenes: load projection params from the extended struct.
  bool is_iso = (current_scene_def->type == SCENE_TYPE_ISOMETRIC);
  if (is_iso) {
    const gba_iso_scene_def_t *iso_def =
        (const gba_iso_scene_def_t *)current_scene_def;
    iso_tile_w   = iso_def->iso_tile_w > 0 ? iso_def->iso_tile_w : 32;
    iso_tile_h   = iso_def->iso_tile_h > 0 ? iso_def->iso_tile_h : 16;
    iso_origin_x = (int16_t)((uint16_t)current_scene_def->width * iso_tile_w / 2);
  } else {
    iso_tile_w   = 32;
    iso_tile_h   = 16;
    iso_origin_x = 0;
  }

  render_scene();

  // Spawn player. For isometric scenes, start_x/start_y are tile coords;
  // for top-down they are tile coords that we convert to pixel coords.
  uint16_t player_x, player_y;
  if (is_iso) {
    player_x = GBA_GAME_DATA.start_x;
    player_y = GBA_GAME_DATA.start_y;
  } else {
    player_x = (uint16_t)GBA_GAME_DATA.start_x * TILE_WIDTH;
    player_y = (uint16_t)GBA_GAME_DATA.start_y * TILE_HEIGHT;
  }
  actor_t *player = spawn_actor(current_scene_def->player_sprite_index,
                                player_x, player_y);
  if (player != NULL) {
    player->move_speed =
        GBA_GAME_DATA.start_move_speed > 0 ? GBA_GAME_DATA.start_move_speed : 1;
    player->anim_speed = GBA_GAME_DATA.start_anim_speed;
    player->collision_enabled = true;
  }

  if (current_scene_def->actors != NULL) {
    for (uint8_t actor_index = 0; actor_index < current_scene_def->actor_count;
         actor_index++) {
      const gba_actor_def_t *actor_def = &current_scene_def->actors[actor_index];
      // Isometric: actor x/y are tile-grid indices (compiler emits them that way).
      // Top-down: actor x/y are already pixel coords.
      uint16_t ax = actor_def->x;
      uint16_t ay = actor_def->y;
      actor_t *actor = spawn_actor(actor_def->sprite_index, ax, ay);
      if (actor == NULL) {
        break;
      }
      actor->move_speed = actor_def->move_speed;
      actor->anim_speed = actor_def->anim_speed;
      actor->collision_enabled = actor_def->collision_enabled;
      actor->persistent = actor_def->persistent;
      actor->pinned = actor_def->pinned;
      actor->hidden = actor_def->hidden;
    }
  }
}

void vm_scene_load(uint8_t scene_index) { load_scene(scene_index); }

void vm_scene_set_tone(uint8_t tone) {
  current_palette_tone = tone & 0x03;
  if (current_scene_def != NULL) {
    render_scene();
  }
}

// --- VM actor/input hooks (called from vm.c) -------------------------------
// Actor indices are runtime indices: 0 is the player, 1..N the scene actors.

uint16_t vm_get_keys(void) { return get_keys(); }

static actor_t *vm_actor(uint8_t actor_index) {
  if (actor_index >= current_scene.num_actors) {
    return NULL;
  }
  actor_t *actor = &actors[actor_index];
  return actor->active ? actor : NULL;
}

void vm_actor_set_position(uint8_t actor_index, uint8_t x, uint8_t y) {
  actor_t *actor = vm_actor(actor_index);
  if (actor == NULL) {
    return;
  }
  actor->x = x;
  actor->y = y;
}

void vm_actor_move_relative(uint8_t actor_index, int8_t dx, int8_t dy) {
  actor_t *actor = vm_actor(actor_index);
  if (actor == NULL) {
    return;
  }
  actor->x = (uint16_t)((int16_t)actor->x + dx);
  actor->y = (uint16_t)((int16_t)actor->y + dy);
}

void vm_actor_set_direction(uint8_t actor_index, uint8_t dir) {
  actor_t *actor = vm_actor(actor_index);
  if (actor == NULL) {
    return;
  }
  actor->dir = dir;
}

void vm_actor_set_hidden(uint8_t actor_index, uint8_t hidden) {
  actor_t *actor = vm_actor(actor_index);
  if (actor == NULL) {
    return;
  }
  actor->hidden = hidden != 0;
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
    actor->movement_type = MOVEMENT_TYPE_STATIC;
    actor->move_speed = 0;
    actor->movement_positive = true;
    actor->bounds_x = x;
    actor->bounds_y = y;
    actor->bounds_w = TILE_WIDTH;
    actor->bounds_h = TILE_HEIGHT;
    actor->movement_bounds_x = 0;
    actor->movement_bounds_y = 0;
    actor->movement_bounds_w = 0;
    actor->movement_bounds_h = 0;

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
