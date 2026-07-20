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
#define OBJ_VRAM GBA_VRAM_ADDR(0x10000u)

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
static bool scene_changed_during_update;

// Isometric projection params (only meaningful when scene type == ISOMETRIC).
// iso_origin_x centres the diamond grid: tile(0,0) appears at the top-centre
// of the canvas rather than the top-left corner.
static uint8_t  iso_tile_w   = 32;  // projected tile width  (screen px)
static uint8_t  iso_tile_h   = 16;  // projected tile height (screen px)
static int16_t  iso_origin_x = 0;
static int16_t  iso_origin_y = 0;

static uint8_t scene_background_width(const gba_scene_def_t *scene) {
  if (scene == NULL) {
    return SCENE_W;
  }
  return scene->background_width > 0 ? scene->background_width : scene->width;
}

static uint8_t scene_background_height(const gba_scene_def_t *scene) {
  if (scene == NULL) {
    return SCENE_H;
  }
  return scene->background_height > 0 ? scene->background_height
                                      : scene->height;
}

static uint16_t iso_projected_width(void) {
  return (uint16_t)(current_scene.width + current_scene.height) *
         (uint16_t)(iso_tile_w / 2);
}

static uint16_t iso_projected_height(void) {
  return (uint16_t)(current_scene.width + current_scene.height) *
         (uint16_t)(iso_tile_h / 2);
}

// Convert isometric tile coordinates to screen pixel coordinates.
// Actors in ISO scenes store tile indices in x/y; this gives OAM position.
static int16_t iso_screen_x(int16_t tile_x, int16_t tile_y) {
  return iso_origin_x + (tile_x - tile_y) * (iso_tile_w / 2);
}
static int16_t iso_screen_y(int16_t tile_x, int16_t tile_y, int8_t iso_z) {
  return iso_origin_y + (tile_x + tile_y) * (iso_tile_h / 2) -
         (int16_t)iso_z * iso_tile_h;
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

// Isometric movement is grid based: one immediate tile step, then a controlled
// repeat while the same direction remains held. Without this, a keyboard tap
// lasting only a few 60 Hz frames skips several trigger/collision cells.
static uint16_t iso_held_direction;
static uint8_t iso_repeat_timer;

#define ISO_SLOWEST_REPEAT_FRAMES 6

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

// Depth-sorted draw order for isometric scenes. GBA objects with equal OBJ
// priority resolve overlap in favour of the *lower OAM index*, so actors
// nearest the viewer must be emitted first. The editor's depth key is
// tile_x + tile_y + iso_z; higher values are in front.
static uint8_t actor_draw_order[MAX_ACTORS];

static void build_iso_draw_order(void) {
  uint8_t count = current_scene.num_actors;
  for (uint8_t i = 0; i < count; i++) {
    actor_draw_order[i] = i;
  }
  // Simple insertion sort — MAX_ACTORS is small (16), so this is fine.
  for (uint8_t i = 1; i < count; i++) {
    uint8_t key = actor_draw_order[i];
    int16_t key_depth = (int16_t)actors[key].x + (int16_t)actors[key].y +
                        actors[key].iso_z;
    int8_t j = (int8_t)i - 1;
    while (j >= 0) {
      uint8_t prev = actor_draw_order[(uint8_t)j];
      int16_t prev_depth = (int16_t)actors[prev].x +
                           (int16_t)actors[prev].y + actors[prev].iso_z;
      if (prev_depth >= key_depth) break;
      actor_draw_order[(uint8_t)(j + 1)] = prev;
      j--;
    }
    actor_draw_order[(uint8_t)(j + 1)] = key;
  }
}

// Compiled scripts and actor definitions use GB Studio's direction order:
// down=0, left=1, right=2, up=3. Sprite animation tables use the engine order
// emitted by compileSprites: down, right, up, left, followed by the four
// moving variants.
static uint8_t actor_animation_index(const actor_t *actor) {
  static const uint8_t direction_to_animation[4] = {0, 3, 1, 2};
  uint8_t direction = actor->dir < 4 ? actor->dir : 0;
  uint8_t animation = direction_to_animation[direction];
  if (actor->vel_x != 0 || actor->vel_y != 0) {
    animation = (uint8_t)(animation + 4);
  }
  return animation;
}

static const gba_metasprite_tile_t *actor_metasprite(
    const gba_sprite_def_t *sprite, const actor_t *actor, uint8_t *tile_count) {
  *tile_count = sprite->metasprite_len;

  if (sprite->frame_count == 0 || sprite->frames == NULL ||
      sprite->frame_lengths == NULL || sprite->anim_count == 0 ||
      sprite->animations == NULL) {
    return sprite->metasprite;
  }

  uint8_t animation_index = actor_animation_index(actor);
  if (animation_index >= sprite->anim_count) {
    animation_index = 0;
  }

  const gba_sprite_anim_t *animation = &sprite->animations[animation_index];
  if (animation->start >= sprite->frame_count ||
      animation->end < animation->start) {
    return sprite->metasprite;
  }

  uint8_t end = animation->end;
  if (end >= sprite->frame_count) {
    end = (uint8_t)(sprite->frame_count - 1);
  }
  uint8_t frame_count = (uint8_t)(end - animation->start + 1);
  uint8_t frame_index =
      (uint8_t)(animation->start + actor->anim_frame % frame_count);
  const gba_metasprite_tile_t *frame = sprite->frames[frame_index];
  if (frame == NULL) {
    return sprite->metasprite;
  }

  *tile_count = sprite->frame_lengths[frame_index];
  return frame;
}

typedef struct metasprite_bounds_t {
  int16_t left;
  int16_t top;
  int16_t right;
  int16_t bottom;
} metasprite_bounds_t;

static metasprite_bounds_t metasprite_bounds(
    const gba_metasprite_tile_t *metasprite, uint8_t metasprite_len,
    uint8_t object_height) {
  metasprite_bounds_t bounds = {0, 0, 0, 0};
  int16_t object_x = 0;
  int16_t object_y = 0;

  for (uint8_t i = 0; i < metasprite_len; i++) {
    object_x = (int16_t)(object_x + metasprite[i].x);
    object_y = (int16_t)(object_y + metasprite[i].y);
    int16_t right = (int16_t)(object_x + 8);
    int16_t bottom = (int16_t)(object_y + object_height);
    if (i == 0 || object_x < bounds.left) bounds.left = object_x;
    if (i == 0 || object_y < bounds.top) bounds.top = object_y;
    if (i == 0 || right > bounds.right) bounds.right = right;
    if (i == 0 || bottom > bounds.bottom) bounds.bottom = bottom;
  }

  return bounds;
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

    uint8_t metasprite_len = 0;
    const gba_metasprite_tile_t *metasprite =
        actor_metasprite(sprite, actor, &metasprite_len);

    // For isometric scenes actor->x/y are tile coords. The projected point is
    // the diamond's top vertex; its ground centre is half a tile lower. Anchor
    // the current metasprite's horizontal centre and bottom edge there so a
    // 16x16 character stands at (centre - 8, diamond top - 8).
    // For top-down actor->x/y remain the legacy metasprite origin in pixels.
    int16_t base_sx, base_sy;
    uint8_t object_height = sprite->obj_8x16 ? 16 : 8;
    if (is_iso) {
      metasprite_bounds_t bounds =
          metasprite_bounds(metasprite, metasprite_len, object_height);
      int16_t ground_x = iso_screen_x((int16_t)actor->x, (int16_t)actor->y);
      int16_t ground_y =
          (int16_t)(iso_screen_y((int16_t)actor->x, (int16_t)actor->y,
                                 actor->iso_z) +
                    iso_tile_h / 2);
      base_sx = (int16_t)(ground_x - (bounds.left + bounds.right) / 2 -
                          (int16_t)camera.x);
      base_sy =
          (int16_t)(ground_y - bounds.bottom - (int16_t)camera.y);
    } else {
      base_sx = (int16_t)actor->x - (int16_t)camera.x;
      base_sy = (int16_t)actor->y - (int16_t)camera.y;
    }

    uint16_t tile_base = loaded_sprite_tile_bases[actor->sprite_index];
    int16_t object_x = 0;
    int16_t object_y = 0;
    for (uint8_t tile_index = 0; tile_index < metasprite_len; tile_index++) {
      if (oam_index >= MAX_OAM_ENTRIES) {
        return;
      }

      const gba_metasprite_tile_t *tile = &metasprite[tile_index];
      object_x = (int16_t)(object_x + tile->x);
      object_y = (int16_t)(object_y + tile->y);
      int16_t screen_x = (int16_t)(base_sx + object_x);
      int16_t screen_y = (int16_t)(base_sy + object_y);

      if (screen_x <= -8 || screen_x >= SCREEN_WIDTH ||
          screen_y <= -(int16_t)object_height ||
          screen_y >= SCREEN_HEIGHT) {
        hide_oam_entry(oam_index++);
        continue;
      }

      volatile uint16_t *oam = MEM_OAM + oam_index * 4u;
      oam[0] = (uint16_t)(screen_y & 0x00FF);
      if (sprite->obj_8x16) {
        // OBJ shape=vertical, size=0 => 8x16.
        oam[0] |= 0x8000u;
      }
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
  uint8_t background_width = scene_background_width(scene);
  uint8_t background_height = scene_background_height(scene);
  uint16_t width = background_width < 32 ? background_width : 32;
  uint16_t height = background_height < 32 ? background_height : 32;

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
        uint16_t map_index = y * background_width + x;
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
  iso_held_direction = 0;
  iso_repeat_timer = 0;
  iso_tile_w = 32;
  iso_tile_h = 16;
  iso_origin_x = 0;
  iso_origin_y = 0;

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
  scene_changed_during_update = false;
  uint16_t keys = get_keys();
  const bool gameplay_input_was_locked = textbox_is_open() || VM_ISLOCKED();
  if (!gameplay_input_was_locked && (keys & KEY_START)) {
    load_scene((uint8_t)((current_scene_index + 1) % GBA_GAME_DATA.scene_count));
  }

  (void)script_runner_update();
  // Preserve the pre-update state so the A press that closes a textbox cannot
  // fall through and immediately start another nearby interaction. Also lock
  // input when the runner opened a textbox during this frame.
  const bool gameplay_input_locked = gameplay_input_was_locked ||
                                     scene_changed_during_update ||
                                     textbox_is_open() || VM_ISLOCKED();

  // Actor interaction: when A is pressed and no textbox or script is blocking,
  // find the nearest interactive non-player actor. Isometric interaction is
  // cardinal grid adjacency (or the same tile); top-down retains its legacy
  // one-8px-tile box.
  if (key_pressed(KEY_A) && current_scene.num_actors > 0 && actors[0].active &&
      !gameplay_input_locked) {
    const actor_t *player = &actors[0];
    const uint8_t *nearest_script = NULL;
    int16_t nearest_distance = INT16_MAX;
    for (uint8_t ai = 1; ai < current_scene.num_actors; ai++) {
      const actor_t *other = &actors[ai];
      if (!other->active || other->hidden || other->disabled) {
        continue;
      }
      if (current_scene_def == NULL || current_scene_def->actors == NULL ||
          ai - 1 >= current_scene_def->actor_count) {
        continue;
      }
      const uint8_t *script = current_scene_def->actors[ai - 1].interact_script;
      if (script == NULL) {
        continue;
      }
      int16_t dx = (int16_t)other->x - (int16_t)player->x;
      int16_t dy = (int16_t)other->y - (int16_t)player->y;
      if (dx < 0) dx = (int16_t)-dx;
      if (dy < 0) dy = (int16_t)-dy;
      bool in_range = current_scene.type == SCENE_TYPE_ISOMETRIC
                          ? (dx + dy <= 1)
                          : (dx <= TILE_WIDTH && dy <= TILE_HEIGHT);
      int16_t distance = (int16_t)(dx + dy);
      if (in_range && distance < nearest_distance) {
        nearest_script = script;
        nearest_distance = distance;
      }
    }
    if (nearest_script != NULL) {
      script_execute(0, (UBYTE *)nearest_script, NULL, 0);
    }
  }

  if (current_scene.num_actors > 0 && actors[0].active) {
    actor_t *player = &actors[0];
    int16_t step = (int16_t)(player->move_speed > 0 ? player->move_speed : 1);
    player->vel_x = 0;
    player->vel_y = 0;

    if (!gameplay_input_locked && current_scene.type == SCENE_TYPE_ISOMETRIC) {
      // Isometric D-pad mapping (tile-grid units):
      //   UP    → NE: tile_y--  → screen_x+, screen_y-
      //   DOWN  → SW: tile_y++  → screen_x-, screen_y+
      //   LEFT  → NW: tile_x--  → screen_x-, screen_y-
      //   RIGHT → SE: tile_x++  → screen_x+, screen_y+
      // Choose one grid axis even when the keyboard reports multiple held
      // directions. This keeps every input step on a collision/trigger cell
      // and matches the four diagonal isometric sprite directions.
      uint16_t direction_key = 0;
      if (keys & KEY_UP) direction_key = KEY_UP;
      else if (keys & KEY_DOWN) direction_key = KEY_DOWN;
      else if (keys & KEY_LEFT) direction_key = KEY_LEFT;
      else if (keys & KEY_RIGHT) direction_key = KEY_RIGHT;

      bool should_step = false;
      if (direction_key == 0) {
        iso_held_direction = 0;
        iso_repeat_timer = 0;
      } else if (direction_key != iso_held_direction) {
        should_step = true;
        iso_held_direction = direction_key;
      } else if (iso_repeat_timer == 0) {
        should_step = true;
      } else {
        iso_repeat_timer--;
      }

      if (should_step) {
        // move_speed controls repeat cadence rather than tile distance. A
        // default speed of 1 repeats every six frames; higher values repeat
        // faster but never skip an intermediate collision cell.
        uint8_t speed = player->move_speed > 0 ? player->move_speed : 1;
        uint8_t repeat_frames = speed >= ISO_SLOWEST_REPEAT_FRAMES
                                    ? 1
                                    : (uint8_t)(ISO_SLOWEST_REPEAT_FRAMES + 1 -
                                                speed);
        iso_repeat_timer = (uint8_t)(repeat_frames - 1);
        if (direction_key == KEY_UP) player->vel_y = -1;
        else if (direction_key == KEY_DOWN) player->vel_y = 1;
        else if (direction_key == KEY_LEFT) player->vel_x = -1;
        else if (direction_key == KEY_RIGHT) player->vel_x = 1;
      }
    } else if (!gameplay_input_locked) {
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
      iso_held_direction = 0;
      iso_repeat_timer = 0;
    } else if (current_scene.type == SCENE_TYPE_ISOMETRIC) {
      // Dialogue/scripts consume movement and reset held-repeat state so a
      // key used to dismiss text cannot cause a delayed grid step.
      iso_held_direction = 0;
      iso_repeat_timer = 0;
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

    if (current_scene.type == SCENE_TYPE_ISOMETRIC) {
      // Isometric sprite slots describe projected diagonals. Map logical-grid
      // movement to the GB Studio direction value whose compiled animation
      // faces that projected direction:
      //   tile_y-- NE => up,   tile_y++ SW => down,
      //   tile_x-- NW => left, tile_x++ SE => right.
      // Studio emits isometric animations in runtime table order
      // [SW, SE, NE, NW], matching [down, right, up, left].
      if (actor->vel_y < 0) actor->dir = 3;       // NE / up
      else if (actor->vel_y > 0) actor->dir = 0;  // SW / down
      else if (actor->vel_x < 0) actor->dir = 1;  // NW / left
      else if (actor->vel_x > 0) actor->dir = 2;  // SE / right
    } else if (actor->vel_y > 0) {
      actor->dir = 0; // down
    } else if (actor->vel_x < 0) {
      actor->dir = 1; // left
    } else if (actor->vel_x > 0) {
      actor->dir = 2; // right
    } else if (actor->vel_y < 0) {
      actor->dir = 3; // up
    }

    if (actor->vel_x != 0 || actor->vel_y != 0) {
      if (current_scene.type == SCENE_TYPE_ISOMETRIC &&
          current_scene_def != NULL) {
        // Isometric coordinates are logical cells. Resolve one cell at a time
        // so speeds above one can never tunnel through a solid tile. Map
        // bounds remain impassable even when collision data is absent or an
        // actor has solid-tile collision disabled.
        int16_t remaining_x = actor->vel_x;
        int16_t step_x = remaining_x > 0 ? 1 : -1;
        while (remaining_x != 0) {
          int16_t next_x = (int16_t)actor->x + step_x;
          int16_t tile_y = (int16_t)actor->y;
          if (next_x < 0 || next_x >= (int16_t)current_scene.width) break;
          if (actor->collision_enabled && current_scene_def->collisions != NULL &&
              current_scene_def->collisions[(uint16_t)tile_y *
                                                current_scene.width +
                                            (uint16_t)next_x] != 0) {
            break;
          }
          actor->x = (uint16_t)next_x;
          remaining_x = (int16_t)(remaining_x - step_x);
        }

        int16_t remaining_y = actor->vel_y;
        int16_t step_y = remaining_y > 0 ? 1 : -1;
        while (remaining_y != 0) {
          int16_t tile_x = (int16_t)actor->x;
          int16_t next_y = (int16_t)actor->y + step_y;
          if (next_y < 0 || next_y >= (int16_t)current_scene.height) break;
          if (actor->collision_enabled && current_scene_def->collisions != NULL &&
              current_scene_def->collisions[(uint16_t)next_y *
                                                current_scene.width +
                                            (uint16_t)tile_x] != 0) {
            break;
          }
          actor->y = (uint16_t)next_y;
          remaining_y = (int16_t)(remaining_y - step_y);
        }
      } else if (actor->collision_enabled && current_scene_def != NULL) {
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
  if (!scene_changed_during_update && current_scene_def != NULL &&
      current_scene_def->triggers != NULL &&
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
      cam_py = iso_screen_y((int16_t)actors[0].x, (int16_t)actors[0].y,
                            actors[0].iso_z);
      // Keep the projected logical grid and compiled background in one shared
      // pixel coordinate system. Either may be larger for custom scenes.
      uint16_t projected_w = iso_projected_width();
      uint16_t projected_h = iso_projected_height();
      uint16_t background_w =
          (uint16_t)scene_background_width(current_scene_def) * TILE_WIDTH;
      uint16_t background_h =
          (uint16_t)scene_background_height(current_scene_def) * TILE_HEIGHT;
      uint16_t world_w = projected_w > background_w ? projected_w : background_w;
      uint16_t world_h = projected_h > background_h ? projected_h : background_h;
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
  scene_changed_during_update = true;

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
  iso_held_direction = 0;
  iso_repeat_timer = 0;

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
    // A W×H diamond spans (W+H)*half_tile_w and starts H half-tiles left
    // of tile (0,0). Centre it in the compiled background when that canvas is
    // larger (the 8×7 demo grid is 240×120 inside a 240×160 image).
    uint16_t projected_w = iso_projected_width();
    uint16_t projected_h = iso_projected_height();
    uint16_t background_w =
        (uint16_t)scene_background_width(current_scene_def) * TILE_WIDTH;
    uint16_t background_h =
        (uint16_t)scene_background_height(current_scene_def) * TILE_HEIGHT;
    uint16_t padding_x = background_w > projected_w
                             ? (uint16_t)((background_w - projected_w) / 2)
                             : 0;
    uint16_t padding_y = background_h > projected_h
                             ? (uint16_t)((background_h - projected_h) / 2)
                             : 0;
    iso_origin_x = (int16_t)(padding_x +
                             (uint16_t)current_scene_def->height *
                                 (iso_tile_w / 2));
    iso_origin_y = (int16_t)padding_y;
  } else {
    iso_tile_w   = 32;
    iso_tile_h   = 16;
    iso_origin_x = 0;
    iso_origin_y = 0;
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
    player->dir = GBA_GAME_DATA.start_direction;
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
      actor->dir = actor_def->direction;
      actor->collision_enabled = actor_def->collision_enabled;
      actor->persistent = actor_def->persistent;
      actor->pinned = actor_def->pinned;
      actor->hidden = actor_def->hidden;
      actor->iso_z = actor_def->iso_z;
    }
  }

  // Match GB Studio's scene lifecycle: run the scene script once after the
  // player and actors exist. Scheduling a fresh context is safe even when a
  // VM_OP_LOAD_SCENE initiated this load; the caller finishes its current
  // context while this scene-start context begins on the next runner update.
  if (current_scene_def->start_script != NULL) {
    script_execute(0, (UBYTE *)current_scene_def->start_script, NULL, 0);
  }
}

void vm_scene_load(uint8_t scene_index) { load_scene(scene_index); }

void vm_scene_load_at(uint8_t scene_index, uint8_t x, uint8_t y,
                      uint8_t direction) {
  if (GBA_GAME_DATA.scene_count == 0) {
    return;
  }

  load_scene(scene_index);
  if (current_scene.num_actors == 0 || !actors[0].active) {
    return;
  }

  actor_t *player = &actors[0];
  uint8_t tile_x = current_scene.width > 0 && x >= current_scene.width
                       ? (uint8_t)(current_scene.width - 1)
                       : x;
  uint8_t tile_y = current_scene.height > 0 && y >= current_scene.height
                       ? (uint8_t)(current_scene.height - 1)
                       : y;

  if (current_scene.type == SCENE_TYPE_ISOMETRIC) {
    player->x = tile_x;
    player->y = tile_y;
  } else {
    player->x = (uint16_t)tile_x * TILE_WIDTH;
    player->y = (uint16_t)tile_y * TILE_HEIGHT;
  }
  player->bounds_x = player->x;
  player->bounds_y = player->y;
  player->vel_x = 0;
  player->vel_y = 0;
  player->dir = direction < 4 ? direction : 0;
}

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

void vm_actor_set_collisions(uint8_t actor_index, uint8_t enabled) {
  actor_t *actor = vm_actor(actor_index);
  if (actor == NULL) {
    return;
  }
  actor->collision_enabled = enabled != 0;
}

bool vm_actor_at_position(uint8_t actor_index, uint8_t x, uint8_t y) {
  const actor_t *actor = vm_actor(actor_index);
  return actor != NULL && actor->x == x && actor->y == y;
}

bool vm_actor_is_relative(uint8_t actor_index, uint8_t other_actor_index,
                          uint8_t direction) {
  const actor_t *actor = vm_actor(actor_index);
  const actor_t *other = vm_actor(other_actor_index);
  if (actor == NULL || other == NULL) {
    return false;
  }

  switch (direction) {
  case 0: // down / below
    return actor->y > other->y;
  case 1: // left
    return actor->x < other->x;
  case 2: // right
    return actor->x > other->x;
  case 3: // up / above
    return actor->y < other->y;
  default:
    return false;
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
    actor->iso_z = 0;
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
