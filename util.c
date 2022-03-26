#include "util.h"

static u32 rndv;

void srand_xorshift(u16 seed) {
	rndv = seed;
}

int rand_xorshift() {
	rndv = rndv ^ (rndv << 13);
	rndv = rndv ^ (rndv >> 17);
	rndv = rndv ^ (rndv << 5);
	return rndv & 0xffff;
}
