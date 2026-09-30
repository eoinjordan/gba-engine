#ifndef TANG_H
#define TANG_H
#include <stdint.h>
void tang_present(void);
void tang_render_frame(void);
#define TANG_FRAMEBUFFER ((volatile uint16_t *)0x10000000u)
#define TANG_LCD_FRAME (*(volatile uint32_t *)0x80000000u)
#define TANG_GAME_FRAME (*(volatile uint32_t *)0x80000004u)
#define TANG_DRAW (*(volatile uint32_t *)0x80000008u)
#define TANG_RENDERER (*(volatile uint32_t *)0x8000000cu)
#endif
