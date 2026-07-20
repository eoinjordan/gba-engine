#ifndef ENGINE_H
#define ENGINE_H

#include "gba_types.h"

// Engine lifecycle functions
void engine_init(void);
void engine_update(void);
void engine_render(void);
void engine_run(void);

// Scene management
void load_scene(uint8_t scene_index);
void vm_scene_load(uint8_t scene_index);
void vm_scene_load_at(uint8_t scene_index, uint8_t x, uint8_t y,
                      uint8_t direction);
void vm_scene_set_tone(uint8_t tone);

// Actor management
actor_t* spawn_actor(uint8_t sprite_index, uint16_t x, uint16_t y);
void destroy_actor(actor_t* actor);
void vm_actor_set_position(uint8_t actor_index, uint8_t x, uint8_t y);
void vm_actor_move_relative(uint8_t actor_index, int8_t dx, int8_t dy);
void vm_actor_set_direction(uint8_t actor_index, uint8_t dir);
void vm_actor_set_hidden(uint8_t actor_index, uint8_t hidden);
void vm_actor_set_collisions(uint8_t actor_index, uint8_t enabled);
bool vm_actor_at_position(uint8_t actor_index, uint8_t x, uint8_t y);
bool vm_actor_is_relative(uint8_t actor_index, uint8_t other_actor_index,
                          uint8_t direction);

#endif // ENGINE_H
