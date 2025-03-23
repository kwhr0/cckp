#include "types.h"

#define VRAM		((u16 *)0x140000)
#define SCREEN_PX	640
#define SCREEN_PY	480
#define FONT_YN		12

#define BLACK		0
#define BLUE		0x001f
#define GREEN		0x07e0
#define RED			0xf800
#define WHITE		0xffff

void init();
void cls();
void color(u16 color);
void locate(u8 x, u8 y);
void setPrinter(u8 f);
void putchar(char c);
void puts_n(char *p);
void cursorOn();
void cursorOff();
void _log(const char *format, ...);
u8 keyDown(u8 repeat);
void sleep();
void memset_align(AINT *dst, AINT value, AINT size);
void memcpy_align(AINT *dst, AINT *src, AINT size);

extern u16 curColor;
