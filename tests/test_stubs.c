#include "test_stubs.h"
#include <stdbool.h>

int stub_scene_load_calls = 0;
uint8_t stub_last_scene_index = 0;

int stub_scene_tone_calls = 0;
uint8_t stub_last_scene_tone = 0;

int stub_textbox_open_calls = 0;
const char *stub_last_textbox_text = NULL;
bool stub_textbox_dismiss_next = false;

void stub_reset(void) {
  stub_scene_load_calls = 0;
  stub_last_scene_index = 0;
  stub_scene_tone_calls = 0;
  stub_last_scene_tone = 0;
  stub_textbox_open_calls = 0;
  stub_last_textbox_text = NULL;
  stub_textbox_dismiss_next = false;
}

// vm.c declares these `extern` and calls them directly when it executes
// VM_OP_LOAD_SCENE / VM_OP_SET_SCENE_TONE. On hardware, engine.c provides the
// real implementations (which load tiles/palettes into VRAM); here we just
// record the call so tests can assert on VM dispatch behaviour in isolation.
void vm_scene_load(uint8_t scene_index) {
  stub_scene_load_calls++;
  stub_last_scene_index = scene_index;
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
