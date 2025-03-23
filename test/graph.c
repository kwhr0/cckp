#include "graph.h"
#include "base.h"

void point(u16 x, u16 y) {
	if (x < SCREEN_PX && y < SCREEN_PY) VRAM[SCREEN_PX * y + x] = curColor;
}

void line(u16 x0, u16 y0, u16 x1, u16 y1) {
	u16 x = x0, y = y0, dx, dy, f;
	s16 d, sx, sy;
	if (x0 < x1) {
		dx = x1 - x0;
		sx = 1;
	}
	else {
		dx = x0 - x1;
		sx = -1;
	}
	if (y0 < y1) {
		dy = y1 - y0;
		sy = 1;
	}
	else {
		dy = y0 - y1;
		sy = -1;
	}
	if (dx > dy) {
		d = dx >> 1;
		do {
			point(x, y);
			if ((d += dy) >= dx) {
				d -= dx;
				y += sy;
			}
			f = x != x1;
			x += sx;
		} while(f);
	}
	else {
		d = dy >> 1;
		do {
			point(x, y);
			if ((d += dx) >= dy) {
				d -= dy;
				x += sx;
			}
			f = y != y1;
			y += sy;
		} while (f);
	}
}

void box(u16 x0, u16 y0, u16 x1, u16 y1) {
	line(x0, y0, x1, y0);
	line(x0, y1, x1, y1);
	line(x0, y0, x0, y1);
	line(x1, y0, x1, y1);
}

void boxfill(u16 x0, u16 y0, u16 x1, u16 y1) {
	if (x0 >= SCREEN_PX) x0 = SCREEN_PX - 1;
	if (x1 >= SCREEN_PX) x1 = SCREEN_PX - 1;
	if (x0 > x1) {
		u16 t = x0;
		x0 = x1;
		x1 = t;
	}
	if (y0 >= SCREEN_PY) y0 = SCREEN_PY - 1;
	if (y1 >= SCREEN_PY) y1 = SCREEN_PY - 1;
	if (y0 > y1) {
		u16 t = y0;
		y0 = y1;
		y1 = t;
	}
	u16 *p = VRAM + SCREEN_PX * y0 + x0;
	u16 a = SCREEN_PX - (x1 - x0 + 1);
	for (u16 y = y0; y <= y1; y++) {
		for (u16 x = x0; x <= x1; x++) *p++ = curColor;
		p += a;
	}
}

void circle(u16 x0, u16 y0, u16 r) {
	if (!r) return;
	u16 x = r, y = 0;
	s16 f = (-r << 1) + 3;
	while (x >= y) {
		point(x0 + x, y0 + y);
		point(x0 - x, y0 + y);
		point(x0 + x, y0 - y);
		point(x0 - x, y0 - y);
		point(x0 + y, y0 + x);
		point(x0 - y, y0 + x);
		point(x0 + y, y0 - x);
		point(x0 - y, y0 - x);
		if (f >= 0) {
			x--;
			f -= x << 2;
		}
		y++;
		f += (y << 2) + 2;
	}
}
