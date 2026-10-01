#include "environment.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint16_t vram[49152],colors[512],oam[512],fb[38400];
uint16_t test_control,test_bg0,test_bg1,test_sx0,test_sy0,test_sx1,test_sy1;
volatile uint32_t lcd_frames,game_frames;
static void reset(void) {
    memset(vram,0,sizeof(vram)); memset(colors,0,sizeof(colors));
    memset(oam,0,sizeof(oam)); memset(fb,0,sizeof(fb));
    for (unsigned i=0;i<128;++i) oam[i*4]=0x0200;
    test_control=BG0_ENABLE; test_bg0=(28<<8)|1; test_bg1=(30<<8)|4;
    test_sx0=test_sy0=test_sx1=test_sy1=0;
    colors[0]=7; colors[1]=RGB15(31,0,0); colors[2]=RGB15(0,31,0);
    colors[257]=RGB15(0,0,31);
    vram[28*1024]=1;
    ((uint32_t *)vram)[8]=0x22222221;
    ((uint32_t *)vram)[15]=0x11111112;
}
int main(void) {
    reset(); tang_render_frame();
    assert(fb[0]==colors[1] && fb[1]==colors[2] && fb[8]==colors[0]);
    assert(fb[159*240+239]==colors[0]);
    vram[28*1024]=1|0x400; tang_render_frame();
    assert(fb[0]==colors[2] && fb[7]==colors[1]);
    vram[28*1024]=1|0x800; tang_render_frame();
    assert(fb[0]==colors[2] && fb[1]==colors[1]);
    vram[28*1024]=1; test_sx0=1; tang_render_frame();
    assert(fb[0]==colors[2] && fb[7]==colors[0]);
    test_sx0=0; test_control|=BG1_ENABLE;
    vram[30*1024]=1; ((uint32_t *)vram)[0x4000/4+8]=0x00000002;
    tang_render_frame(); assert(fb[0]==colors[2] && fb[1]==colors[2]);
    test_control=BG0_ENABLE|OBJ_ENABLE|OBJ_1D_MAP;
    oam[0]=0; oam[1]=0; oam[2]=1;
    ((uint32_t *)vram)[0x10000/4+8]=0x00000001;
    tang_render_frame(); assert(fb[0]==colors[257] && fb[1]==colors[2]);
    oam[2]=1|(2<<10); tang_render_frame(); assert(fb[0]==colors[1]);
    oam[0]=2<<14; oam[2]=1;
    ((uint32_t *)vram)[0x10000/4+16]=0x11111111;
    tang_render_frame(); assert(fb[8*240]==colors[257]);
    puts("PASS: framebuffer bounds, background flips/scroll, text transparency, sprite priority/8x16");
    return 0;
}
