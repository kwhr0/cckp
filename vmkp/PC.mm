#import "PC.h"
#import "MyDocument.h"

PC::PC() : doc(nil) {
	Open();
}

PC::~PC() {
	if (fi) fclose(fi);
}

void PC::Open() {
	if (fi) fclose(fi);
	fi = fopen((std::string(getenv("HOME")) + "/fat.dmg").c_str(), "rb");
}

uint8_t PC::input(adr_t adr) {
	int32_t r = 0;
	switch (adr & 0xf) {
		case 0:
			r = doc.keydata;
			break;
		case 1:
			r = doc.keymod;
			break;
		case 4:
			r = fi ? getc(fi) : 0;
			break;
		case 5:
			r = !fi || feof(fi);
			break;
	}
	return r;
}

void PC::output(adr_t adr, uint8_t data) {
	switch (adr & 0xf) {
		case 0:
			putchar(data);
			break;
		case 2:
			psg_adr = data;
			break;
		case 3:
			psg.Set(psg_adr, data);
			break;
		case 4:
			sector = data;
			break;
		case 5:
			sector |= data << 8;
			break;
		case 6:
			sector |= data << 16;
			if (fi) fseek(fi, sector << 9, SEEK_SET);
			break;
	}
}

void PSG::Mute() {
	memset(op, 0, sizeof(op));
}

void PSG::Set(uint8 adr, uint8 data) {
	Op *p = &op[adr >> 3];
	switch (adr & 7) {
		case 0:
			p->delta_tmp = data;
			break;
		case 1:
			p->delta = data << 8 | p->delta_tmp;
			break;
		case 2:
			p->vtmp = data;
			break;
		case 3:
			p->vl = p->vtmp;
			p->vr = data;
			break;
		case 4:
			p->type = data & 3;
			p->count = 0;
			break;
	}
	
}

#define P() puts("PSG: clipped")

void PSG::Update(sint16 &l, sint16 &r) {
	int al = 0, ar = 0;
	for (int i = 0; i < N; i++) {
		Op *p = &op[i];
		if (!p->vl && !p->vr) continue;
		sint16 t = 0;
		if ((p->delta & 0xff00) != 0xff00) {
			p->count += p->delta;
			switch (p->type) {
				default:
					t = p->count & 0x8000 ? -0x4000 : 0x4000;
					break;
				case 1:
					t = p->count & 0xe000 ? -0x4000 : 0x4000;
					break;
				case 2:
					t = (0x7fff - p->count) >> 1;
					break;
				case 3:
					t = p->count << 1 & 0x7fff;
					if (p->count & 0x4000) t = 0x7fff - t;
					if (p->count & 0x8000) t = -t;
					break;
			}
		}
		else {
			rng = rng >> 1 | ((rng >> 2 ^ rng >> 3) & 1) << 16;
			t = rng & 1 ? -0x2000 : 0x2000;
		}
		al += t * p->vl;
		ar += t * p->vr;
	}
	al >>= ATT;
	ar >>= ATT;
	if (al < -0x8000) { al = -0x8000; P(); }
	else if (al > 0x7fff) { al = 0x7fff; P(); }
	if (ar < -0x8000) { ar = -0x8000; P(); }
	else if (ar > 0x7fff) { ar = 0x7fff; P(); }
	l = al;
	r = ar;
}
