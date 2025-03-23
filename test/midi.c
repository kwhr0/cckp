#include "midi.h"
#include "snd.h"
#include "file.h"

#include <stdio.h>


#define MIDI_N		16

enum {
	INIT, NOTEON2, CONTROL2, PITCH2, META, META2, DUMMY, DUMMYV, 
	// following line must be assigned 8 to 15
	NOTEOFF, NOTEON, KEY, CONTROL, PROGRAM, CHANNEL, PITCH, EX, 
	TEMPO, TEMPO2, TEMPO3
};

typedef struct { // hard coded offset of volume, pan, expression
	u8 volume, rpnl, rpnm, pan, expression, prognum;
	s8 bend, bendsen;
} Midi;

static Midi sMidi[MIDI_N];
static u32 sTimebase;
static s32 sTime; // S23.8
static u16 sDelta;
static u8 sState, sState0, sStarted;

void MidiInit() {
	Midi *p;
	for (p = sMidi; p < sMidi + MIDI_N; p++) {
		p->prognum = p->bend = 0;
		p->volume = 100;
		p->pan = 64;
		p->expression = p->rpnl = p->rpnm = 127;
		p->bendsen = 2;
	}
	sTime = 0;
	sState = sState0 = INIT;
	sStarted = 0;
}

static void Note(u8 midi_ch, u8 note, u8 velo) {
	u16 id = note << 8 | midi_ch;
	if (velo) {
		Midi *p = &sMidi[midi_ch];
		SndKeyOn(p->prognum, note, velo, p->volume * p->expression >> 6, p->pan, p->bendsen * p->bend, id);
	}
	else SndKeyOff(id);
}

static u8 Process1(u8 data) {
	static u32 v;
	static u16 len;
	static u8 ch, d1;
	Midi *p = &sMidi[ch];
	if (sState == INIT && !(data & 0x80)) sState = sState0;
	u32 v2;
	switch (sState) {
		case INIT:
		ch = data & 0xf;
		len = 0;
		if (data >= 0x80 && data <= 0xf0) sState = sState0 = data >> 4;
		else if (data == 0xff) sState = sState0 = META;
		break;
		case NOTEOFF:
		Note(ch, data, 0);
		sState = DUMMY;
		break;
		case NOTEON:
		d1 = data;
		sState = NOTEON2;
		break;
		case NOTEON2:
		Note(ch, d1, data);
		sStarted = 1;
		sState = INIT;
		break;
		case CONTROL:
		d1 = data;
		sState = CONTROL2;
		break;
		case CONTROL2:
		switch (d1) {
			case 7: case 10: case 11:
			((u8 *)p)[d1 - 7] = data;
			SndVolex(ch, p->volume * p->expression >> 6, p->pan);
			break;
			case 98: case 99:
			p->rpnl = p->rpnm = 127;
			break;
			case 100:
			p->rpnl = data;
			break;
			case 101:
			p->rpnm = data;
			break;
			case 6:
			if (!p->rpnl && !p->rpnm) p->bendsen = data & 0xf;
			break;
		}
		sState = INIT;
		break;
		case PROGRAM:
		p->prognum = data;
		sState = INIT;
		break;
		case KEY:
		sState = DUMMY;
		break;
		case PITCH:
		d1 = data;
		sState = PITCH2;
		break;
		case PITCH2:
		p->bend = (data << 1 | d1 >> 6) - 0x80;
		SndBend(ch, p->bendsen * p->bend);
		sState = INIT;
		break;
		case EX:
		len = len << 7 | data & 0x7f;
		if (!(data & 0x80)) sState = len ? DUMMYV : INIT;
		break;
		case META:
		d1 = data;
		sState = META2;
		break;
		case META2:
		len = len << 7 | data & 0x7f;
		if (!(data & 0x80)) sState = d1 == 0x51 ? TEMPO : len ? DUMMYV : INIT;
		break;
		case TEMPO:
		v = (u32)data << 16;
		sState = TEMPO2;
		break;
		case TEMPO2:
		v |= (u16)data << 8;
		sState = TEMPO3;
		break;
		case TEMPO3:
		v |= data;
		v2 = (u32)(sTime << 8) / sDelta;
		sDelta = sTimebase / v;
		sTime = v2 * sDelta >> 8;
		sState = INIT;
		break;
		case DUMMYV:
		if (!--len) sState = INIT;
		break;
		case CHANNEL:
		default:
		sState = INIT;
		break;
	}
	return sState != INIT;
}

u8 MidiUpdate() {
	while (sTime >= 0) {
		s16 c;
		while ((c = FileGetChar()) >= 0 && Process1(c)) 
			;
		if (c < 0) return 1;
		u16 r = 0;
		do {
			c = FileGetChar();
			if (c < 0) return 1;
			r = r << 7 | c & 0x7f;
		} while (c & 0x80);
		sTime -= (u32)r << 8;
	}
	sTime = sStarted ? sTime + sDelta : 0;
	return 0;
}

u8 MidiHeader() {
	u8 i;
	for (i = 0; i < 12; i++) if (FileGetChar() < 0) return 1;
	s16 c;
	if ((c = FileGetChar()) < 0) return 1;
	u32 v = c << 8;
	if ((c = FileGetChar()) < 0) return 1;
	v |= c;
	sTimebase = INTERVAL * v << 8;
	sDelta = sTimebase / 500000;	// default: 0.5sec BPM=120
	for (i = 0; i < 5; i++) if (FileGetChar() < 0) return 1;
	if ((c = FileGetChar()) < 0) return 1;
	v = c << 16;
	if ((c = FileGetChar()) < 0) return 1;
	v |= c << 8;
	if ((c = FileGetChar()) < 0) return 1;
	v |= c;
	FileSetRemain(v);
	do {
		c = FileGetChar();
		if (c < 0) return 1;
	} while (c & 0x80);
	return 0;
}
