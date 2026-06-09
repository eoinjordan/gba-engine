#ifndef GBA_TYPES_H
#define GBA_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// GBA specific includes
#include "gba_system.h"

// Screen dimensions
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160

// Tile dimensions  
#define TILE_WIDTH  8
#define TILE_HEIGHT 8

// Map dimensions in tiles
#define MAP_WIDTH  (SCREEN_WIDTH / TILE_WIDTH)
#define MAP_HEIGHT (SCREEN_HEIGHT / TILE_HEIGHT)

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

// Movement types (GB Studio's per-actor movement-pattern system): a compiled
// scene assigns each non-player actor one of these so the engine can drive
// its velocity without a script running every frame. See movement.{c,h} for
// the pure, host-testable logic that computes velocity from these.
#define MOVEMENT_TYPE_STATIC 0  // Never moves under its own power.
#define MOVEMENT_TYPE_PATROL 1  // Paces back and forth across `bounds`.
#define MOVEMENT_TYPE_FOLLOW 2  // Walks toward a target while within range.

typedef enum {
    LCD_simple,
    LCD_parallax,
    LCD_fullscreen
} LCD_isr_e;

typedef struct animation_t
{
    uint8_t start;
    uint8_t end;
} animation_t;

typedef struct actor_t
{
    bool active               : 1;
    bool pinned               : 1;
    bool hidden               : 1;
    bool disabled             : 1;
    bool anim_noloop          : 1;
    bool collision_enabled    : 1;
    bool movement_interrupt   : 1;
    bool persistent           : 1;
    // Runtime-only patrol state for MOVEMENT_TYPE_PATROL: true while the
    // actor is moving toward the "positive" end of its bounds (right when
    // patrolling horizontally, down when patrolling vertically). Not part of
    // any compiled data format — purely engine bookkeeping between frames.
    bool movement_positive    : 1;
    uint8_t sprite_index;
    uint8_t palette_index;
    uint16_t x;
    uint16_t y;
    int16_t vel_x;
    int16_t vel_y;
    uint8_t anim_frame;
    uint8_t anim_speed;
    uint8_t anim_tick;
    uint8_t collision_group;
    uint8_t movement_type;
    // Pixels/frame for MOVEMENT_TYPE_PATROL/MOVEMENT_TYPE_FOLLOW (see
    // movement.{c,h}). Unused (and harmless) for MOVEMENT_TYPE_STATIC.
    uint8_t move_speed;
    uint16_t bounds_x;
    uint16_t bounds_y;
    uint16_t bounds_w;
    uint16_t bounds_h;
    // Movement-pattern parameters, interpreted per `movement_type`:
    //   MOVEMENT_TYPE_PATROL: a pixel-space rectangle the actor paces back
    //     and forth across (see movement_patrol's bounds_* parameters).
    //   MOVEMENT_TYPE_FOLLOW: only movement_bounds_w is used, as the
    //     actor's square aggro range in pixels (see movement_follow's
    //     `range` parameter); the other three fields are ignored.
    //   MOVEMENT_TYPE_STATIC: ignored entirely.
    uint16_t movement_bounds_x;
    uint16_t movement_bounds_y;
    uint16_t movement_bounds_w;
    uint16_t movement_bounds_h;
} actor_t;

typedef struct scene_t
{
    uint8_t width;
    uint8_t height;
    uint8_t type;
    uint8_t num_actors;
    uint8_t num_triggers;
    uint8_t num_projectiles;
    uint16_t background_index;
    uint16_t palette_index;
    actor_t *actors;
    // Add other scene data as needed
} scene_t;

typedef struct sprite_t
{
    uint8_t width;
    uint8_t height;
    uint8_t num_frames;
    uint8_t *data;
} sprite_t;

typedef struct palette_t
{
    uint16_t colors[16]; // GBA uses 16-bit colors
} palette_t;

#endif // GBA_TYPES_H
