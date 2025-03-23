#include "types.h"

#ifdef ISA64

static inline int isinf(float x) {
	u64 v = *(u64 *)&x;
	return (v >> 52 & 0x7ff) == 0x7ff && !(v & 0xfffffffffffff);
}

static inline int isnan(float x) {
	u64 v = *(u64 *)&x;
	return (v >> 52 & 0x7ff) == 0x7ff && v & 0xfffffffffffff;
}

#else

static inline int isinf(float x) {
	u32 v = *(u32 *)&x;
	return (v >> 23 & 0xff) == 0xff && !(v & 0x7fffff);
}

static inline int isnan(float x) {
	u32 v = *(u32 *)&x;
	return (v >> 23 & 0xff) == 0xff && v & 0x7fffff;
}

#endif
