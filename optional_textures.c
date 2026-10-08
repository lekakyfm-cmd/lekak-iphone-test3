/* Optional PC texture dump/pack services are disabled for this bring-up.
 * Rendering, VRAM, packets and GTE use the real engine implementations. */
#include "pc/render/texture_dump.h"
int TextureDump_Enabled;
uint32_t *TextureDump_Tags;
uint16_t *TextureDump_Shadow;
void (*TextureDump_Paint)(int,int,int,int);
int (*TextureDump_Prepare)(int,int,int,int,int,int,int);
int (*TextureDump_Sample)(int,int,int,int,int,uint32_t *);
void TextureDump_Init(void) {}
void TextureDump_Loaded(int x,int y,int w,int h,const uint16_t *p)
{(void)x;(void)y;(void)w;(void)h;(void)p;}
void TextureDump_Moved(int x,int y,int dx,int dy,int w,int h)
{(void)x;(void)y;(void)dx;(void)dy;(void)w;(void)h;}
void TextureDump_Cleared(int x,int y,int w,int h)
{(void)x;(void)y;(void)w;(void)h;}
void TextureDump_Primitive(const uint16_t *p,int x,int y,int d,int cx,int cy,int u0,int v0,int u1,int v1)
{(void)p;(void)x;(void)y;(void)d;(void)cx;(void)cy;(void)u0;(void)v0;(void)u1;(void)v1;}
