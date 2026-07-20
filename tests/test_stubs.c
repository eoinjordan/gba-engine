#include "test_stubs.h"
#include <stdbool.h>

int stub_scene_load_calls = 0;
uint8_t stub_last_scene_index = 0;
int stub_scene_load_at_calls = 0;
uint8_t stub_last_scene_x = 0;
uint8_t stub_last_scene_y = 0;
uint8_t stub_last_scene_direction = 0;

int stub_scene_tone_calls = 0;
uint8_t stub_last_scene_tone = 0;

int stub_textbox_open_calls = 0;
const char *stub_last_textbox_text = NULL;
bool stub_textbox_dismiss_next = false;

uint16_t stub_keys = 0;
int stub_actor_set_pos_calls = 0;
uint8_t stub_last_actor_index = 0;
uint8_t stub_last_actor_x = 0;
uint8_t stub_last_actor_y = 0;
int stub_actor_move_rel_calls = 0;
int8_t stub_last_actor_dx = 0;
int8_t stub_last_actor_dy = 0;
int stub_actor_set_dir_calls = 0;
uint8_t stub_last_actor_dir = 0;
int stub_actor_set_hidden_calls = 0;
uint8_t stub_last_actor_hidden = 0;
int stub_actor_set_collisions_calls = 0;
uint8_t stub_last_actor_collisions_enabled = 0;
bool stub_actor_at_position_result = false;
int stub_actor_at_position_calls = 0;
bool stub_actor_relative_result = false;
int stub_actor_relative_calls = 0;
uint8_t stub_last_other_actor_index = 0;
uint8_t stub_last_actor_relation = 0;

void stub_reset(void) {
  stub_scene_load_calls = 0;
  stub_last_scene_index = 0;
  stub_scene_load_at_calls = 0;
  stub_last_scene_x = 0;
  stub_last_scene_y = 0;
  stub_last_scene_direction = 0;
  stub_scene_tone_calls = 0;
  stub_last_scene_tone = 0;
  stub_textbox_open_calls = 0;
  stub_last_textbox_text = NULL;
  stub_textbox_dismiss_next = false;
  stub_keys = 0;
  stub_actor_set_pos_calls = 0;
  stub_last_actor_index = 0;
  stub_last_actor_x = 0;
  stub_last_actor_y = 0;
  stub_actor_move_rel_calls = 0;
  stub_last_actor_dx = 0;
  stub_last_actor_dy = 0;
  stub_actor_set_dir_calls = 0;
  stub_last_actor_dir = 0;
  stub_actor_set_hidden_calls = 0;
  stub_last_actor_hidden = 0;
  stub_actor_set_collisions_calls = 0;
  stub_last_actor_collisions_enabled = 0;
  stub_actor_at_position_result = false;
  stub_actor_at_position_calls = 0;
  stub_actor_relative_result = false;
  stub_actor_relative_calls = 0;
  stub_last_other_actor_index = 0;
  stub_last_actor_relation = 0;
}

// vm.c declares these `extern` and calls them directly when it executes
// VM_OP_LOAD_SCENE / VM_OP_SET_SCENE_TONE. On hardware, engine.c provides the
// real implementations (which load tiles/palettes into VRAM); here we just
// record the call so tests can assert on VM dispatch behaviour in isolation.
void vm_scene_load(uint8_t scene_index) {
  stub_scene_load_calls++;
  stub_last_scene_index = scene_index;
}

void vm_scene_load_at(uint8_t scene_index, uint8_t x, uint8_t y,
                      uint8_t direction) {
  stub_scene_load_at_calls++;
  stub_last_scene_index = scene_index;
  stub_last_scene_x = x;
  stub_last_scene_y = y;
  stub_last_scene_direction = direction;
}

void vm_scene_set_tone(uint8_t tone) {
  stub_scene_tone_calls++;
  stub_last_scene_tone = tone;
}

// Textbox stubs — the real implementations in textbox.c touch GBA VRAM/
// registers; here we just record calls so VM opcode tests can assert on
// dispatch behaviour without needing any hardware.
void textbox_open(const char *text) {
  stub_textbox_open_calls++;
  stub_last_textbox_text = text;
}

bool textbox_update(void) {
  return stub_textbox_dismiss_next;
}

void textbox_close(void) {}
void textbox_init(void) {}

// Input / actor hooks — vm.c calls these for VM_OP_IF_INPUT and the
// VM_OP_ACTOR_* opcodes. Record calls so tests assert VM dispatch in isolation.
uint16_t vm_get_keys(void) { return stub_keys; }

void vm_actor_set_position(uint8_t actor, uint8_t x, uint8_t y) {
  stub_actor_set_pos_calls++;
  stub_last_actor_index = actor;
  stub_last_actor_x = x;
  stub_last_actor_y = y;
}

void vm_actor_move_relative(uint8_t actor, int8_t dx, int8_t dy) {
  stub_actor_move_rel_calls++;
  stub_last_actor_index = actor;
  stub_last_actor_dx = dx;
  stub_last_actor_dy = dy;
}

void vm_actor_set_direction(uint8_t actor, uint8_t dir) {
  stub_actor_set_dir_calls++;
  stub_last_actor_index = actor;
  stub_last_actor_dir = dir;
}

void vm_actor_set_hidden(uint8_t actor, uint8_t hidden) {
  stub_actor_set_hidden_calls++;
  stub_last_actor_index = actor;
  stub_last_actor_hidden = hidden;
}

void vm_actor_set_collisions(uint8_t actor, uint8_t enabled) {
  stub_actor_set_collisions_calls++;
  stub_last_actor_index = actor;
  stub_last_actor_collisions_enabled = enabled;
}

bool vm_actor_at_position(uint8_t actor, uint8_t x, uint8_t y) {
  stub_actor_at_position_calls++;
  stub_last_actor_index = actor;
  stub_last_actor_x = x;
  stub_last_actor_y = y;
  return stub_actor_at_position_result;
}

bool vm_actor_is_relative(uint8_t actor, uint8_t other_actor, uint8_t dir) {
  stub_actor_relative_calls++;
  stub_last_actor_index = actor;
  stub_last_other_actor_index = other_actor;
  stub_last_actor_relation = dir;
  return stub_actor_relative_result;
}
