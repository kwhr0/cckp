#include "base.h"
#include "graph.h"
#include "midi.h"
#include "snd.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define ENTRY_N		300
#define DEPTH_N		4
#define NAME_LEN	13

typedef struct {
	u8 name[NAME_LEN];
	u8 dir;
} Entry;

static Entry entry[ENTRY_N];
static u16 entryN;
static u16 entryIndex[DEPTH_N];
static u8 autokey;
static DIR dir;
static FILINFO info;

FIL file;
u8 int_snd;

void interrupt1() {
	int_snd = 1;
}

static int cmp(const void *a, const void *b) {
	return strcmp(((Entry *)a)->name, ((Entry *)b)->name);
}

static void draw() {
	u16 x, y;
	for (y = 8 * FONT_YN; y < (8 + 32) * FONT_YN; y += 2 * FONT_YN) {
		color(WHITE);
		for (x = 40; x < 640; x += 8) boxfill(x, y - 5, x + 6, y + 15);
		color(BLACK);
		u16 c = 0;
		for (x = 42; x < 640; x += 8) {
			if (c != 2 && c != 6) boxfill(x + 3, y - 5, x + 7, y + 5);
			if (++c >= 7) c = 0;
		}
	}
	color(WHITE);
}

static s8 list() {
	static u8 opened, depth;
	locate(70, 0);
	printf("0-9A-Z   jump file");
	locate(70, 1);
	printf("[<][>]   change file");
	locate(70, 2);
	printf("[return] directory down or play");
	locate(70, 3);
	printf("[esc]    directory up or exit");
	if (opened) {
		opened = 0;
		f_close(&file);
		locate(depth + 15, depth);
		puts_n("       ");
	}
	if (f_opendir(&dir, ".") != FR_OK) return -1;
	u16 i;
	for (i = 0; i < ENTRY_N && f_readdir(&dir, &info) == FR_OK && *info.fname;) {
		if (info.fattrib & AM_DIR) {
			entry[i].dir = 1;
			strcpy(entry[i++].name, info.fname);
		}
		else if (strstr(info.fname, ".MID")) {
			entry[i].dir = 0;
			strcpy(entry[i++].name, info.fname);
		}
	}
	if (f_closedir(&dir) != FR_OK) return -1;
	entryN = i;
	qsort(entry, entryN, sizeof(Entry), cmp);
	i = entryIndex[depth];
	u8 c = 0;
	while (1) {
		locate(depth, depth);
		printf("%-12s", entry[i].name);
		if (autokey) c = autokey;
		else while (!(c = keyDown(1))) sleep();
		if (isalnum(c)) {
			c = toupper(c);
			for (i = 0; i < entryN && c > *entry[i].name; i++)
				;
		}
		else if (c == 29) {
			if (i > 0) {
				i--;
				if (autokey) autokey = 10;
			}
			else autokey = 0;
		}
		else if (c == 28) {
			if (i < entryN - 1) {
				i++;
				if (autokey) autokey = 10;
			}
			else if (autokey) autokey = 27;
		}
		else if (c == 10) {
			if (!entry[i].dir) {
				opened = 1;
				autokey = 0;
				locate(depth + 15, depth);
				puts_n("PLAYING");
				entryIndex[depth] = i;
				_log("%s", entry[i].name);
				return f_open(&file, entry[i].name, FA_READ) == FR_OK ? 1 : -1;
			}
			else if (depth < DEPTH_N - 1 && f_chdir(entry[i].name) == FR_OK) {
				entryIndex[depth++] = i;
				entryIndex[depth] = 0;
				return 0;
			}
		}
		else if (c == 27)
			if (depth && f_chdir("..") == FR_OK) {
				locate(depth, depth);
				puts_n("            ");
				depth--;
				if (autokey) autokey = 28;
				return 0;
			}
			else return -1;
	}
}

int player() {
	locate(0, 8);
	printf("1\n\n2\n\n3\n\n4\n\n5\n\n6\n\n7\n\n8\n\n9\n\nA\n\nB\n\nC\n\nD\n\nE\n\nF\n\nG");
	draw();
	SndInit();
	if (f_chdir("/MIDI") != FR_OK) return 1;
	u8 endcount = 0;
	while (1) {
		s8 r = list();
		locate(70, 0);
		printf("1-9A-G   enable/disable track");
		locate(70, 1);
		printf("[esc]    stop playing");
		locate(70, 2);
		printf("                               ");
		locate(70, 3);
		printf("                             ");
		if (r > 0) {
			FileInit();
			FileFill();
			MidiInit();
			MidiHeader();
			endcount = 0;
			u16 cnt = 0;
			while (1) {
				if (int_snd) {
					int_snd = 0;
					if (MidiUpdate() && !endcount) endcount = 1;
					SndUpdate();
				}
				u8 c = keyDown(1);
				if (c >= '1' && c <= '9') {
					color(SndToggleMute(c - '1') ? BLUE : WHITE);
					locate(0, 8 + ((c - '1') << 1));
					xputc(c);
				}
				else if (c >= 'a' && c <= 'g') {
					color(SndToggleMute(c - 'a' + 9) ? BLUE : WHITE);
					locate(0, 8 + ((c - 'a' + 9) << 1));
					xputc(c - 32);
				}
				else if (c == 27) {
					autokey = 0;
					break;
				}
				else if (c == 28 || c == 29) {
					autokey = c;
					break;
				}
				else if (endcount && ++endcount >= 30) {
					autokey = 28;
					break;
				}
				if (int_snd) _log("OVER %d", ++cnt);
				else {
					FileUpdate();
					sleep();
				}
			}
			SndInit();
			draw();
		}
		else if (r < 0) return 1;
	}
	return 0;
}
