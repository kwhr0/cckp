#include "base.h"
#include "graph.h"
#include "ff.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

static FATFS fatfs;

void player();
void viewer();

static int R(int n) {
	return n * (_rand() & 0xffff) >> 16;
}

static void mandelbrot() {
	cls();
	for (s8 y = -19; y <= 19; y++) {
		for (u8 x = 0; x < 100; x++) {
			u8 i = 0;
			float f = 0, g = 0;
			do {
				float t = f;
				f = t * t - g * g - 2.f + x / 30.f;
				g = 2.f * t * g + y / 15.f;
			} while (++i <= 111 && f * f + g * g < 11.f);
			color((i & 4 ? RED : 0) | (i & 2 ? GREEN : 0) | (i & 1 ? BLUE : 0));
			putchar('#');
		}
		putchar('\n');
	}
}

void main() {
	init();
	_srand(12345);
	if (f_mount(&fatfs, "0:", 0) != FR_OK) return 1;
	while (1) {
		cls();
		color(WHITE);
		printf("D: display\nM: mandelbrot\nP: player\nV: viewer\n\nSelect> ");
		cursorOn();
		int c, i;
		do if (!(c = keyDown(1))) sleep();
		while (c != 'd' && c != 'm' && c != 'p' && c != 'v');
		cls();
		switch (c) {
		case 'd':
			for (i = 0; i < 100; i++) {
				color(_rand());
				for (c = 0x20; c < 0x80; c++) putchar(c);
			}
			cls();
			for (i = 0; i < 1000; i++) {
				color(_rand());
				line(R(640), R(480), R(640), R(480));
			}
			cls();
			for (i = 0; i < 1000; i++) {
				color(_rand());
				box(R(640), R(480), R(640), R(480));
			}
			cls();
			for (i = 0; i < 100; i++) {
				color(_rand());
				boxfill(R(640), R(480), R(640), R(480));
			}
			cls();
			for (i = 0; i < 1000; i++) {
				color(_rand());
				circle(R(640), R(480), R(200));
			}
			cls();
			break;
		case 'm':
			mandelbrot();
			break;
		case 'p':
			player();
			break;
		case 'v':
			viewer();
			break;
		}
	}
	return 0;
}
