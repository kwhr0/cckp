#include "types.h"
#include "base.h"
#include "tjpgd.h"
#include <stdio.h>

#if JD_FASTDECODE == 2
#define WORKSIZE	10000
#else
#define WORKSIZE	3500
#endif

extern FIL file;

static char work[WORKSIZE];
static DIR dir;
static FILINFO info;
static JDEC jdec;
static int xofs, yofs, key;

static UINT infunc(JDEC *j, BYTE *buf, UINT n) {
	if (buf) {
		UINT bytesread;
		f_read(&file, buf, n, &bytesread);
		return bytesread;
	}
	else {
		f_lseek(&file, f_tell(&file) + n);
		return n;
	}
}

static UINT outfunc(JDEC *j, void *buf, JRECT *rect) {
	int x0 = xofs + rect->left, y0 = yofs + rect->top;
	int x1 = xofs + rect->right, y1 = yofs + rect->bottom;
	u16 *sp = (u16 *)buf, *dp = VRAM + SCREEN_PX * y0 + x0;
	for (int y = y0; y <= y1; y++) {
		if (y >= 0 && y < SCREEN_PY) {
			for (int x = x0; x <= x1; x++) {
				if (x >= 0 && x < SCREEN_PX) *dp = *sp;
				sp++;
				dp++;
			}
			dp += SCREEN_PX - (x1 - x0 + 1);
		}
		else if (y < SCREEN_PY + 16) dp += SCREEN_PX;
		else return 0;
	}
	return !(key = keyDown(0));
}

void viewer() {
	if (f_chdir("/JPEG") != FR_OK) return;
	if (f_opendir(&dir, ".") != FR_OK) {
		f_chdir("/");
		return;
	}
	key = 0;
	while (f_readdir(&dir, &info) == FR_OK && *info.fname) {
		if (!strstr(info.fname, ".jpg") && !strstr(info.fname, ".JPG")) continue;
		if (f_open(&file, info.fname, FA_READ) != FR_OK) continue;
		if (jd_prepare(&jdec, infunc, work, sizeof(work), 0) != JDR_OK) {
			f_close(&file);
			continue;
		}
		int i;
		for (i = 0; i < 3 &&
			(jdec.width >> i) > SCREEN_PX && (jdec.height >> i) > SCREEN_PY;
			i++)
			;
		xofs = SCREEN_PX - (jdec.width >> i) >> 1;
		yofs = SCREEN_PY - (jdec.height >> i) >> 1;
		if (yofs < 0) yofs = 0;
		jd_decomp(&jdec, outfunc, i);
		f_close(&file);
		if (key) break;
	}
	f_closedir(&dir);
	f_chdir("/");
}
