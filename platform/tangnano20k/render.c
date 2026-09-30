// Mode 0 subset used by GBA-engine: BG0/BG1, 4bpp tiles, regular sprites.
#include "gba_system.h"
#include "tang.h"

#define FAST __attribute__((section(".fast")))
static uint16_t palette[512] FAST;
static uint16_t row[240] FAST;
static uint8_t priority[240] FAST;
static uint32_t frames FAST;
static struct sprite { uint16_t a,b,c; } active[128];
static unsigned active_count;

static uint16_t tile_color(unsigned base, unsigned tile, unsigned x, unsigned y) {
    const volatile uint32_t *words = (const volatile uint32_t *)MEM_VRAM;
    uint32_t pixels = words[(base + tile*32 + y*4)/4];
    return (uint16_t)((pixels >> (x*4)) & 15);
}

static void background(unsigned layer, unsigned y, uint16_t control,
                       unsigned sx, unsigned sy) {
    unsigned ty = (y+sy)&255;
    unsigned charbase=((control>>2)&3)*0x4000;
    unsigned mapbase=((control>>8)&31)*0x800;
    for (unsigned x=0;x<240;) {
        unsigned tx=(x+sx)&255;
        uint16_t entry=MEM_VRAM[(mapbase/2)+(ty/8)*32+tx/8];
        unsigned px=tx&7, py=ty&7;
        unsigned bank=(entry>>12)*16;
        const volatile uint32_t *words=(const volatile uint32_t *)MEM_VRAM;
        unsigned ry=entry&0x800 ? 7-py : py;
        uint32_t pixels=words[(charbase+(entry&1023)*32+ry*4)/4];
        unsigned count=8-px;
        if (count>240-x) count=240-x;
        for (unsigned n=0;n<count;++n,++x,++px) {
            unsigned rx=entry&0x400 ? 7-px : px;
            unsigned p=(pixels>>(rx*4))&15;
            if (p) {
                row[x]=palette[bank+p];
                priority[x]=(uint8_t)((control&3)*2+layer);
            }
        }
    }
}

static void sprites(unsigned y) {
    // Reverse order makes the lower OAM index win a priority tie.
    for (unsigned i=0;i<active_count;++i) {
        uint16_t a=active[i].a, b=active[i].b, c=active[i].c;
        unsigned shape=a>>14, size=b>>14;
        static const uint8_t widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
        static const uint8_t heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
        if (shape==3) continue;
        unsigned w=widths[shape][size], h=heights[shape][size];
        unsigned py=(y-(a&255))&255;
        if (py>=h) continue;
        if (b&0x2000) py=h-1-py;
        int origin=(int)(b&511); if (origin>=256) origin-=512;
        unsigned bank=256+(c>>12)*16;
        unsigned pr=((c>>10)&3)*2;
        for (unsigned dx=0;dx<w;++dx) {
            int x=origin+(int)dx;
            if (x<0 || x>=240 || pr>priority[x]) continue;
            unsigned px=b&0x1000 ? w-1-dx : dx;
            unsigned tile=(c&1023)+(py/8)*(w/8)+px/8;
            unsigned p=tile_color(0x10000,tile,px&7,py&7);
            if (p) { row[x]=palette[bank+p]; priority[x]=(uint8_t)pr; }
        }
    }
}

void tang_render_frame(void) {
    for (unsigned i=0;i<512;++i) palette[i]=MEM_PALETTE[i];
    active_count=0;
    for (int i=127;i>=0;--i) {
        uint16_t a=MEM_OAM[i*4];
        if (a&0x0300 || a&0x2000 || ((a>>10)&3)) continue;
        active[active_count].a=a;
        active[active_count].b=MEM_OAM[i*4+1];
        active[active_count++].c=MEM_OAM[i*4+2];
    }
    uint16_t mode=REG_DISPCNT, bg0=REG_BG0CNT, bg1=REG_BG1CNT;
    unsigned sx0=REG_BG0HOFS, sy0=REG_BG0VOFS;
    unsigned sx1=REG_BG1HOFS, sy1=REG_BG1VOFS;
    for (unsigned y=0;y<160;++y) {
        for (unsigned x=0;x<240;++x) { row[x]=palette[0]; priority[x]=255; }
        // Engine assigns BG1 priority 0 and BG0 priority 1.
        if (mode&BG0_ENABLE) background(0,y,bg0,sx0,sy0);
        if (mode&BG1_ENABLE) background(1,y,bg1,sx1,sy1);
        if (mode&OBJ_ENABLE) sprites(y);
        volatile uint32_t *out=(volatile uint32_t *)(TANG_FRAMEBUFFER+y*240);
        for (unsigned x=0;x<240;x+=2)
            *out++=(uint32_t)row[x] | ((uint32_t)row[x+1]<<16);
    }
}

void tang_present(void) {
    if (TANG_RENDERER==0x54475231u) {
        TANG_DRAW=1;
        while (TANG_DRAW&1) {}
    } else tang_render_frame();
    TANG_GAME_FRAME=++frames;
    uint32_t frame=TANG_LCD_FRAME;
    while (TANG_LCD_FRAME==frame) {}
}
