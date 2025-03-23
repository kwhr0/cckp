#include "types.h"

#ifdef ISA64
#define _srand(x)	srand(x)
#define _rand()		rand()
#else
#define _srand(x)	srand_xorshift(x)
#define _rand()		rand_xorshift()
#endif

void srand_xorshift(u16 seed);
int rand_xorshift();
