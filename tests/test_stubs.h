// Test doubles for the engine-side hooks that vm.c calls out to
// (vm_scene_load / vm_scene_set_tone). On real hardware these live in
// engine.c and touch VRAM/palette directly; for host-side VM tests we record
// what was requested so assertions can check the VM dispatched correctly,
// without needing a GBA (or a mock PPU).

#ifndef GBA_ENGINE_TEST_STUBS_H
#define GBA_ENGINE_TEST_STUBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern int stub_scene_load_calls;
extern uint8_t stub_last_scene_index;

extern int stub_scene_tone_calls;
extern uint8_t stub_last_scene_tone;

extern int stub_textbox_open_calls;
extern const char *stub_last_textbox_text;
extern bool stub_textbox_dismiss_next;

// Input / actor stubs for VM_OP_IF_INPUT and VM_OP_ACTOR_* dispatch tests.
extern uint16_t stub_keys;
extern int stub_actor_set_pos_calls;
extern uint8_t stub_last_actor_index;
extern uint8_t stub_last_actor_x;
extern uint8_t stub_last_actor_y;
extern int stub_actor_move_rel_calls;
extern int8_t stub_last_actor_dx;
extern int8_t stub_last_actor_dy;
extern int stub_actor_set_dir_calls;
extern uint8_t stub_last_actor_dir;
extern int stub_actor_set_hidden_calls;
extern uint8_t stub_last_actor_hidden;
extern int stub_actor_set_collisions_calls;
extern uint8_t stub_last_actor_collisions_enabled;
extern bool stub_actor_at_position_result;
extern int stub_actor_at_position_calls;
extern bool stub_actor_relative_result;
extern int stub_actor_relative_calls;
extern uint8_t stub_last_other_actor_index;
extern uint8_t stub_last_actor_relation;

void stub_reset(void);

#endif // GBA_ENGINE_TEST_STUBS_H
