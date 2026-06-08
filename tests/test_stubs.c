#include "test_stubs.h"

int stub_scene_load_calls = 0;
uint8_t stub_last_scene_index = 0;

int stub_scene_tone_calls = 0;
uint8_t stub_last_scene_tone = 0;

void stub_reset(void) {
  stub_scene_load_calls = 0;
  stub_last_scene_index = 0;
  stub_scene_tone_calls = 0;
  stub_last_scene_tone = 0;
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
