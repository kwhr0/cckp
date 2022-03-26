#include "file.h"
#include "ff.h"
#include "base.h"

#define BUF_N	2048

extern FIL file;

static u8 sBuf[BUF_N];
static u8 *sBufRP, *sBufWP, *sBufEP;
static u16 sBufN;
static u8 sError;
static s32 sRemain;

void FileInit() {
	sBufRP = sBufWP = sBuf;
	sBufEP = 0;
	sBufN = 0;
	sError = 0;
	sRemain = -1;
}

void FileUpdate() {
	if (!sError && !sBufEP && sBufN <= BUF_N - 512) {
		UINT br;
		if (f_read(&file, sBufWP, 512, &br) != FR_OK) {
			sError = 1;
			return;
		}
		sBufN += br;
		if (br < 512) sBufEP = sBufWP + br;
		else if ((sBufWP += 512) >= sBuf + BUF_N) sBufWP = sBuf;
	}
}

void FileFill() {
	for (u8 i = 0; i < BUF_N / 512; i++) FileUpdate();
}

s16 FileGetChar() {
	if (sError || sBufRP == sBufEP || !sRemain) return -1;
	if (!sBufN) {
		_log("UNDERRUN");
		FileUpdate();
	}
	if (!sBufN) return -1;
	sBufN--;
	u8 c = *sBufRP++;
	if (sBufRP >= sBuf + BUF_N) sBufRP = sBuf;
	if (sRemain > 0) sRemain--;
	return c;
}

void FileSetRemain(s32 r) {
	sRemain = r;
}
