#ifndef _TYPES_H_
#define _TYPES_H_

#define ISA64

#ifndef nil
#define nil	0
#endif

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#ifdef ISA64
typedef u64 AINT;
#else
typedef u32 AINT;
#endif

#define IOBASE		0x1dfff0

#define KEYDATA		(*(u8 *)(IOBASE + 0))
#define KEYMOD		(*(u8 *)(IOBASE + 1))
#define STREAM		(*(u8 *)(IOBASE + 4))
#define STREAM_EOF	(*(u8 *)(IOBASE + 5))

#define STDOUT		(*(u8 *)(IOBASE + 0))
#define PSG_ADR		(*(u8 *)(IOBASE + 2))
#define PSG_DATA	(*(u8 *)(IOBASE + 3))
#define LBA0		(*(u8 *)(IOBASE + 4))
#define LBA1		(*(u8 *)(IOBASE + 5))
#define LBA2		(*(u8 *)(IOBASE + 6))

#endif
