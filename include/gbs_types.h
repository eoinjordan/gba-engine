#ifndef GBA_GBS_TYPES_H
#define GBA_GBS_TYPES_H

#include "bankdata.h"
#include <stdbool.h>
#include <stdint.h>

#if __has_include("data/scene_types.h")
#include "data/scene_types.h"
#else
typedef enum scene_type_e {
  SCENE_TYPE_TOPDOWN,
  SCENE_TYPE_PLATFORM,
  SCENE_TYPE_ADVENTURE,
  SCENE_TYPE_SHMUP,
  SCENE_TYPE_POINTNCLICK,
  SCENE_TYPE_LOGO,
} scene_type_e;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef RGB
#define RGB(R, G, B)                                                           \
  ((uint16_t)(((R) & 0x1f) | (((G) & 0x1f) << 5) | (((B) & 0x1f) << 10)))
#endif

#define DMG_BLACK 0x03
#define DMG_DARK_GRAY 0x02
#define DMG_LITE_GRAY 0x01
#define DMG_WHITE 0x00

#ifndef DMG_PALETTE
#define DMG_PALETTE(C0, C1, C2, C3)                                            \
  ((uint8_t)((((C3) & 0x03) << 6) | (((C2) & 0x03) << 4) |                    \
             (((C1) & 0x03) << 2) | ((C0) & 0x03)))
#endif

#define CGB_PALETTE(C0, C1, C2, C3)                                            \
  { C0, C1, C2, C3 }
#define CGB_COLOR(R, G, B) RGB(R, G, B)

#define COLLISION_GROUP_NONE 0x0
#define COLLISION_GROUP_PLAYER 0x1
#define COLLISION_GROUP_1 0x2
#define COLLISION_GROUP_2 0x4
#define COLLISION_GROUP_3 0x8
#define COLLISION_GROUP_MASK 0xF

#define COLLISION_GROUP_FLAG_1 0x10
#define COLLISION_GROUP_FLAG_2 0x20
#define COLLISION_GROUP_FLAG_3 0x40
#define COLLISION_GROUP_FLAG_4 0x80
#define COLLISION_GROUP_FLAG_PLATFORM COLLISION_GROUP_FLAG_3
#define COLLISION_GROUP_FLAG_SOLID COLLISION_GROUP_FLAG_4

typedef enum direction_e {
  DIR_DOWN,
  DIR_UP,
  DIR_LEFT,
  DIR_RIGHT,
} direction_e;

typedef enum LCD_isr_e {
  LCD_simple,
  LCD_parallax,
  LCD_fullscreen,
} LCD_isr_e;

typedef struct point8_t {
  int8_t x;
  int8_t y;
} point8_t;

typedef struct point16_t {
  int16_t x;
  int16_t y;
} point16_t;

typedef struct upoint16_t {
  uint16_t x;
  uint16_t y;
} upoint16_t;

typedef struct rect16_t {
  int16_t left;
  int16_t right;
  int16_t top;
  int16_t bottom;
} rect16_t;

typedef struct urect16_t {
  uint16_t left;
  uint16_t right;
  uint16_t top;
  uint16_t bottom;
} urect16_t;

typedef struct metasprite_t {
  int8_t dy;
  int8_t dx;
  uint8_t dtile;
  uint8_t props;
} metasprite_t;

typedef struct animation_t {
  uint8_t start;
  uint8_t end;
} animation_t;

#define PARALLAX_STEP(START, END, SPEED)                                       \
  { START, END, SPEED }

typedef struct parallax_row_t {
  uint8_t start;
  uint8_t end;
  int8_t speed;
} parallax_row_t;

typedef struct trigger_t {
  uint8_t left, right, top, bottom;
  far_ptr_t script;
  uint8_t script_flags;
} trigger_t;

#define TRIGGER_HAS_ENTER_SCRIPT 1
#define TRIGGER_HAS_LEAVE_SCRIPT 2

typedef struct actor_t {
  bool active : 1;
  bool pinned : 1;
  bool hidden : 1;
  bool disabled : 1;
  bool anim_noloop : 1;
  bool collision_enabled : 1;
  bool movement_interrupt : 1;
  bool persistent : 1;
  upoint16_t pos;
  direction_e dir;
  rect16_t bounds;
  uint8_t base_tile;
  uint8_t frame;
  uint8_t frame_start;
  uint8_t frame_end;
  uint8_t anim_tick;
  uint8_t move_speed;
  uint8_t animation;
  uint8_t reserve_tiles;
  animation_t animations[8];
  far_ptr_t sprite;
  far_ptr_t script;
  far_ptr_t script_update;
  uint16_t hscript_update;
  uint16_t hscript_hit;
  uint8_t collision_group;
  struct actor_t *next;
  struct actor_t *prev;
} actor_t;

typedef struct background_t {
  uint8_t width;
  uint8_t height;
  far_ptr_t tileset;
  far_ptr_t cgb_tileset;
  far_ptr_t tilemap;
  far_ptr_t cgb_tilemap_attr;
} background_t;

typedef struct scene_t {
  uint8_t width, height;
  scene_type_e type;
  uint8_t n_actors, n_triggers, n_projectiles, n_sprites;
  uint8_t reserve_tiles;
  far_ptr_t player_sprite;
  far_ptr_t background, collisions;
  far_ptr_t palette, sprite_palette;
  far_ptr_t script_init, script_p_hit1;
  far_ptr_t sprites;
  far_ptr_t actors;
  far_ptr_t triggers;
  far_ptr_t projectiles;
  urect16_t scroll_bounds;
  parallax_row_t parallax_rows[3];
} scene_t;

typedef struct tileset_t {
  uint16_t n_tiles;
  uint8_t tiles[];
} tileset_t;

typedef struct spritesheet_t {
  uint8_t n_metasprites;
  point8_t emote_origin;
  metasprite_t *const *metasprites;
  animation_t *animations;
  uint16_t *animations_lookup;
  rect16_t bounds;
  far_ptr_t tileset;
  far_ptr_t cgb_tileset;
} spritesheet_t;

typedef struct projectile_def_t {
  bool anim_noloop : 1;
  bool strong : 1;
  rect16_t bounds;
  far_ptr_t sprite;
  uint8_t life_time;
  uint8_t base_tile;
  animation_t animations[4];
  uint8_t anim_tick;
  uint8_t move_speed;
  uint16_t initial_offset;
  uint8_t collision_group;
  uint8_t collision_mask;
} projectile_def_t;

typedef struct projectile_t {
  upoint16_t pos;
  point16_t delta_pos;
  uint8_t frame;
  uint8_t frame_start;
  uint8_t frame_end;
  projectile_def_t def;
  struct projectile_t *next;
} projectile_t;

#define FONT_RECODE 1
#define FONT_VWF 2
#define FONT_VWF_1BIT 4
#define FONT_RECODE_SIZE_7BIT 0x7fu

typedef struct font_desc_t {
  uint8_t attr, mask;
  const uint8_t *recode_table;
  const uint8_t *widths;
  const uint8_t *bitmaps;
} font_desc_t;

typedef struct scene_stack_item_t {
  far_ptr_t scene;
  upoint16_t pos;
  direction_e dir;
} scene_stack_item_t;

typedef struct menu_item_t {
  uint8_t X, Y;
  uint8_t iL, iR, iU, iD;
} menu_item_t;

typedef struct palette_entry_t {
  uint16_t c0, c1, c2, c3;
} palette_entry_t;

typedef struct palette_t {
  uint8_t mask;
  uint8_t palette[2];
  palette_entry_t cgb_palette[];
} palette_t;

#endif
