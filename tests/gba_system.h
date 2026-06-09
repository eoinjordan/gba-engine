#ifndef TEST_GBA_SYSTEM_H
#define TEST_GBA_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

extern uint16_t test_reg_dispcnt;
extern uint16_t test_reg_dispstat;
extern uint16_t test_reg_vcount;
extern uint16_t test_reg_bg0cnt;
extern uint16_t test_reg_bg1cnt;
extern uint16_t test_reg_bg2cnt;
extern uint16_t test_reg_bg3cnt;
extern uint16_t test_reg_bg0hofs;
extern uint16_t test_reg_bg0vofs;
extern uint16_t test_reg_bg1hofs;
extern uint16_t test_reg_bg1vofs;
extern uint16_t test_reg_bg2hofs;
extern uint16_t test_reg_bg2vofs;
extern uint16_t test_reg_bg3hofs;
extern uint16_t test_reg_bg3vofs;
extern uint16_t test_reg_keyinput;
extern uint32_t test_reg_dma3sad;
extern uint32_t test_reg_dma3dad;
extern uint32_t test_reg_dma3cnt;

extern uint16_t test_mem_palette[256];
extern uint16_t test_mem_vram[0x18000 / 2];
extern uint16_t test_mem_oam[0x400 / 2];

#define REG_DISPCNT test_reg_dispcnt
#define REG_DISPSTAT test_reg_dispstat
#define REG_VCOUNT test_reg_vcount
#define REG_BG0CNT test_reg_bg0cnt
#define REG_BG1CNT test_reg_bg1cnt
#define REG_BG2CNT test_reg_bg2cnt
#define REG_BG3CNT test_reg_bg3cnt

#define REG_BG0HOFS test_reg_bg0hofs
#define REG_BG0VOFS test_reg_bg0vofs
#define REG_BG1HOFS test_reg_bg1hofs
#define REG_BG1VOFS test_reg_bg1vofs
#define REG_BG2HOFS test_reg_bg2hofs
#define REG_BG2VOFS test_reg_bg2vofs
#define REG_BG3HOFS test_reg_bg3hofs
#define REG_BG3VOFS test_reg_bg3vofs

#define REG_KEYINPUT test_reg_keyinput

#define REG_DMA3SAD test_reg_dma3sad
#define REG_DMA3DAD test_reg_dma3dad
#define REG_DMA3CNT test_reg_dma3cnt

#define MEM_PALETTE test_mem_palette
#define MEM_VRAM test_mem_vram
#define MEM_OAM test_mem_oam

#define GBA_VRAM_ADDR(offset) (&test_mem_vram[(offset) / sizeof(uint16_t)])

#define MODE_0 0x0000
#define MODE_1 0x0001
#define MODE_2 0x0002
#define MODE_3 0x0003
#define MODE_4 0x0004
#define MODE_5 0x0005

#define BG0_ENABLE 0x0100
#define BG1_ENABLE 0x0200
#define BG2_ENABLE 0x0400
#define BG3_ENABLE 0x0800
#define OBJ_ENABLE 0x1000

#define KEY_A 0x0001
#define KEY_B 0x0002
#define KEY_SELECT 0x0004
#define KEY_START 0x0008
#define KEY_RIGHT 0x0010
#define KEY_LEFT 0x0020
#define KEY_UP 0x0040
#define KEY_DOWN 0x0080
#define KEY_R 0x0100
#define KEY_L 0x0200

#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

void gba_init(void);
void wait_vblank(void);
void vsync(void);

uint16_t get_keys(void);
bool key_pressed(uint16_t key);
bool key_released(uint16_t key);

void set_mode(uint16_t mode);
void set_palette_color(uint16_t index, uint16_t color);
void load_palette(const uint16_t *palette, uint16_t start, uint16_t count);

void dma_copy(const void *src, void *dest, uint32_t count);
void dma_fill(uint32_t value, void *dest, uint32_t count);

void test_reset_hardware(void);
void test_set_keys(uint16_t keys);

extern unsigned test_wait_vblank_calls;
extern unsigned test_load_palette_calls;

#endif
