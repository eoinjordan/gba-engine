#include "../../../include/gba_system.h"
#undef MEM_VRAM
#undef MEM_PALETTE
#undef MEM_OAM
#undef REG_DISPCNT
#undef REG_BG0CNT
#undef REG_BG1CNT
#undef REG_BG0HOFS
#undef REG_BG0VOFS
#undef REG_BG1HOFS
#undef REG_BG1VOFS
extern uint16_t vram[49152],colors[512],oam[512],fb[38400];
extern uint16_t test_control,test_bg0,test_bg1,test_sx0,test_sy0,test_sx1,test_sy1;
#define MEM_VRAM vram
#define MEM_PALETTE colors
#define MEM_OAM oam
#define REG_DISPCNT test_control
#define REG_BG0CNT test_bg0
#define REG_BG1CNT test_bg1
#define REG_BG0HOFS test_sx0
#define REG_BG0VOFS test_sy0
#define REG_BG1HOFS test_sx1
#define REG_BG1VOFS test_sy1
#define TANG_H
#define TANG_FRAMEBUFFER fb
extern volatile uint32_t lcd_frames,game_frames;
#define TANG_LCD_FRAME lcd_frames
#define TANG_GAME_FRAME game_frames
void tang_render_frame(void);
