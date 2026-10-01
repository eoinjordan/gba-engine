#include "test_framework.h"
#include "gba_system.h"
#define GBA_SYSTEM_H
#include "../src/textbox.c"

uint16_t test_reg_dispcnt, test_reg_bg1cnt;
uint16_t test_mem_palette[256], test_mem_vram[0x18000 / 2];
bool key_pressed(uint16_t key) { (void)key; return false; }
size_t text_format_variables(const char *text, char *out, size_t size) {
  return (size_t)snprintf(out, size, "%s", text);
}
size_t text_word_wrap(const char *text, uint8_t width, char *out, size_t size) {
  (void)width;
  return (size_t)snprintf(out, size, "%s", text);
}

TEST(textbox_tiles_hide_the_world_between_letters) {
  textbox_init();
  const uint8_t *tiles = (const uint8_t *)test_mem_vram + 0x4000;
  bool has_text = false;
  for (unsigned glyph = 0; glyph < 96; ++glyph) {
    for (unsigned byte = 0; byte < 32; ++byte) {
      uint8_t packed = tiles[(glyph + 1) * 32 + byte];
      ASSERT_TRUE((packed & 15) == 1 || (packed & 15) == 2);
      ASSERT_TRUE((packed >> 4) == 1 || (packed >> 4) == 2);
      has_text |= (packed & 15) == 1 || (packed >> 4) == 1;
    }
  }
  ASSERT_TRUE(has_text);
  for (unsigned byte = 0; byte < 32; ++byte) ASSERT_EQ(tiles[32 + byte], 0x22);
  textbox_open("A A");
  ASSERT_TRUE(textbox_is_open());
  ASSERT_EQ(test_mem_vram[0xF000 / 2 + 17 * 32 + 2], 0xF001);
  textbox_close();
  ASSERT_TRUE(!textbox_is_open());
}

int main(void) {
  RUN_TEST(textbox_tiles_hide_the_world_between_letters);
  return TEST_REPORT();
}
