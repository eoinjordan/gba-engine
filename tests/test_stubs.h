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

void stub_reset(void);

#endif // GBA_ENGINE_TEST_STUBS_H
