#include "gba_system.h"
#include "textbox.h"
#include <string.h>

uint16_t test_reg_dispcnt;
uint16_t test_reg_dispstat;
uint16_t test_reg_vcount;
uint16_t test_reg_bg0cnt;
uint16_t test_reg_bg1cnt;
uint16_t test_reg_bg2cnt;
uint16_t test_reg_bg3cnt;
uint16_t test_reg_bg0hofs;
uint16_t test_reg_bg0vofs;
uint16_t test_reg_bg1hofs;
uint16_t test_reg_bg1vofs;
uint16_t test_reg_bg2hofs;
uint16_t test_reg_bg2vofs;
uint16_t test_reg_bg3hofs;
uint16_t test_reg_bg3vofs;
uint16_t test_reg_keyinput;
uint32_t test_reg_dma3sad;
uint32_t test_reg_dma3dad;
uint32_t test_reg_dma3cnt;

uint16_t test_mem_palette[256];
uint16_t test_mem_vram[0x18000 / 2];
uint16_t test_mem_oam[0x400 / 2];

unsigned test_wait_vblank_calls;
unsigned test_load_palette_calls;

static uint16_t keys_current;
static uint16_t keys_previous;
static uint16_t keys_next;
static bool textbox_open_flag;

void test_reset_hardware(void) {
  test_reg_dispcnt = 0;
  test_reg_dispstat = 0;
  test_reg_vcount = 0;
  test_reg_bg0cnt = 0;
  test_reg_bg1cnt = 0;
  test_reg_bg2cnt = 0;
  test_reg_bg3cnt = 0;
  test_reg_bg0hofs = 0;
  test_reg_bg0vofs = 0;
  test_reg_bg1hofs = 0;
  test_reg_bg1vofs = 0;
  test_reg_bg2hofs = 0;
  test_reg_bg2vofs = 0;
  test_reg_bg3hofs = 0;
  test_reg_bg3vofs = 0;
  test_reg_keyinput = 0x03FF;
  test_reg_dma3sad = 0;
  test_reg_dma3dad = 0;
  test_reg_dma3cnt = 0;
  memset(test_mem_palette, 0, sizeof(test_mem_palette));
  memset(test_mem_vram, 0, sizeof(test_mem_vram));
  memset(test_mem_oam, 0, sizeof(test_mem_oam));
  keys_current = 0;
  keys_previous = 0;
  keys_next = 0;
  textbox_open_flag = false;
  test_wait_vblank_calls = 0;
  test_load_palette_calls = 0;
}

void test_reset_environment(void) { test_reset_hardware(); }

void test_set_keys(uint16_t keys) {
  keys_next = keys;
  test_reg_keyinput = (uint16_t)(~keys & 0x03FF);
}

void gba_init(void) {
  memset(test_mem_vram, 0, sizeof(test_mem_vram));
  memset(test_mem_palette, 0, sizeof(test_mem_palette));
  REG_DISPCNT = MODE_0 | BG0_ENABLE;
}

void wait_vblank(void) { test_wait_vblank_calls++; }

void vsync(void) { wait_vblank(); }

uint16_t get_keys(void) {
  keys_previous = keys_current;
  keys_current = keys_next;
  return keys_current;
}

bool key_pressed(uint16_t key) {
  return (keys_current & key) && !(keys_previous & key);
}

bool key_released(uint16_t key) {
  return !(keys_current & key) && (keys_previous & key);
}

void set_mode(uint16_t mode) {
  REG_DISPCNT = (uint16_t)((REG_DISPCNT & 0xFFF8) | (mode & 0x0007));
}

void set_palette_color(uint16_t index, uint16_t color) {
  MEM_PALETTE[index] = color;
}

void load_palette(const uint16_t *palette, uint16_t start, uint16_t count) {
  memcpy(&MEM_PALETTE[start], palette, count * sizeof(uint16_t));
  test_load_palette_calls++;
}

void dma_copy(const void *src, void *dest, uint32_t count) {
  memcpy(dest, src, count);
}

void dma_fill(uint32_t value, void *dest, uint32_t count) {
  uint32_t *words = (uint32_t *)dest;
  for (uint32_t index = 0; index < count; index++) {
    words[index] = value;
  }
}

// Textbox stubs — the real textbox.c writes font tiles to VRAM and controls
// BG1; for integration tests we stub them out to avoid linking hardware-facing
// code that doesn't compile under the host toolchain.
void textbox_init(void) {}
void textbox_open(const char *text) {
  (void)text;
  textbox_open_flag = true;
}
bool textbox_update(void) {
  if (!textbox_open_flag) {
    return true;
  }
  if (key_pressed(KEY_A)) {
    textbox_close();
    return true;
  }
  return false;
}
void textbox_close(void) { textbox_open_flag = false; }
bool textbox_is_open(void) { return textbox_open_flag; }
