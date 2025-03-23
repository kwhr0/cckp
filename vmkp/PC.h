#include "KP.h"

@class MyDocument;

class PSG {
	static const int N = 32, ATT = 10;
	struct Op {
		uint16 count, delta;
		uint8 vl, vr, delta_tmp, vtmp, type;
	};
public:
	PSG() : rng(1 << 16) { Mute(); }
	void Mute();
	void Update(sint16 &l, sint16 &r);
	void Set(uint8 adr, uint8 data);
private:
	Op op[N];
	int rng;
};

class PC : public KP {
public:
	PC();
	~PC();
	void SetDoc(MyDocument *_doc) { doc = _doc; }
	void Reset() { KP::Reset(); psg.Mute(); Open(); }
	void Open();
	int GetPC() const { return pc; }
	void PSGUpdate(short &l, short &r) { psg.Update(l, r); }
private:
	uint8_t input(adr_t adr);
	void output(adr_t adr, uint8_t data);
	MyDocument *doc;
	PSG psg;
	uint8 psg_adr;
	FILE *fi;
	uint32 sector;
};
