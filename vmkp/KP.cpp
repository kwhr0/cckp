#include "KP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IS_IO(adr)	((adr) >= 0x1dfff0 && (adr) <= 0x1dffff)
#define INT_VEC		4
#define INT_VEC_N	7
#define CLOCK(n)	(clock += (n))
#define SHAMT_I		2

enum {
		// 00000ooo xxxxxxxx xxxxxxxx xxxxxxxx
/*00*/	NOP, SLEEP, EI, DI, RET, RETI, _06, _07,
		// 00001ooo rrrrxxxx xxxxxxxx xxxxxxxx
/*08*/	CALLV, COM, NEG, NEGF, CVTF, CVTI, _0E, _0F,
		// 000100oo xxxxxxxx iiiiiiii iiiiiiii
/*10*/	BRA, CALL, _12, _13,
		// 0001oooo rrrrxxxx iiiiiiii iiiiiiii
/*14*/	ADDI, _15, LUI, LWI, ANDI, ORI, XORI, LI, SLL, _1D, SRL, SRA,
		// 001ooooo rrrrrrrr iiiiiiii iiiiiiii
/*20*/	BEQ, BNE, BEQF, BNEF, _24, _25, _26, _27,
		_28, _29, _2A, _2B, _2C, _2D, _2E, LEA,
/*30*/	ST, ST2, ST4, ST8, _3C, _3D, _3E, _3F,
		LD, LD2, LD4, LD8, LDU, LDU2, LDU4, LDU8,
		// 01oooooo rrrrrrrr xxxxxxxx xxxxxxxx
/*40*/	ADD, SUB, MUL, DIV, REM, MULU, DIVU, REMU,
/*48*/	ADDF, SUBF, MULF, DIVF, AND, OR, XOR, MOV,
/*50*/	SLLV, _31, SRLV, SRAV, SEQ, SNE, SEQF, SNEF,
/*58*/	SLT, SLE, SLTU, SLEU, SGT, SGE, SGTU, SGEU,
/*60*/	SLTF, SLEF, SGTF, SGEF,
};

void KP::Reset() {
	pc = 0;
	memset(r, 0, sizeof(r));
	intreq = 0;
	intperm = false;
#if KP_TRACE
	memset(tracebuf, 0, sizeof(tracebuf));
	tracep = tracebuf;
#endif
}

#if DEBUG
void KP::wchk(adr_t adr) {
	if (adr < 0x40000 || adr >= 0x200000) {
		fprintf(stderr, "write to TEXT segment or out of RAM.\n");
#if KP_TRACE
		StopTrace();
#else
		exit(1);
#endif
	}
}
#else
#define wchk(adr)
#endif

#define r1			(r[inst >> 20 & 0xf])
#define r2			(r[inst >> 16 & 0xf])
#define ru1			((data_ut &)r[inst >> 20 & 0xf])
#define ru2			((data_ut &)r[inst >> 16 & 0xf])
#define rf1			((data_ft &)r[inst >> 20 & 0xf])
#define rf2			((data_ft &)r[inst >> 16 & 0xf])
#define setadr()	(adr = int(r2 + (int16_t)inst))

int KP::Execute(int n) {
	clock = 0;
	bool sleepf = false;
	do {
#if KP_TRACE
		tracep->pc = pc;
		tracep->acsR = tracep->acsL = tracep->acsS = 0;
#endif
#if DEBUG
		r[0] = 0;
#endif
		adr_t adr;
		inst_t inst = (inst_t &)m[pc];
		pc += 1 << SHAMT_I;
		switch (inst >> 24) {
			case MOV:
				r1 = r2;
				break;
			case ADD:
				r1 += r2;
				break;
			case ADDI:
				r1 += (int16_t)inst;
				break;
			case ADDF:
				rf1 += rf2;
				break;
			case SUB:
				r1 -= r2;
				break;
			case SUBF:
				rf1 -= rf2;
				break;
			case MUL:
				r1 *= r2;
				CLOCK(4);
				break;
			case MULU:
				ru1 *= ru2;
				CLOCK(4);
				break;
			case MULF:
				rf1 *= rf2;
				CLOCK(4);
				break;
			case DIV:
				r1 /= r2;
				CLOCK(32);
				break;
			case DIVU:
				ru1 /= ru2;
				CLOCK(32);
				break;
			case DIVF:
				rf1 /= rf2;
				CLOCK(32);
				break;
			case REM:
				r1 %= r2;
				CLOCK(32);
				break;
			case REMU:
				ru1 %= ru2;
				CLOCK(32);
				break;
			case NEG:
				r1 = -r1;
				break;
			case NEGF:
				rf1 = -rf1;
				break;
			case COM:
				r1 = ~r1;
				break;
			case AND:
				r1 &= r2;
				break;
			case ANDI:
				r1 &= inst & 0xffff;
				break;
			case OR:
				r1 |= r2;
				break;
			case ORI:
				r1 |= inst & 0xffff;
				break;
			case XOR:
				r1 ^= r2;
				break;
			case SLL:
				r1 <<= inst & 0xffff;
				break;
			case SLLV:
				ru1 <<= ru2;
				break;
			case SRA:
				r1 >>= inst & 0xffff;
				break;
			case SRAV:
				r1 >>= ru2;
				break;
			case SRL:
				ru1 >>= inst & 0xffff;
				break;
			case SRLV:
				ru1 >>= ru2;
				break;
			case LI:
				r1 = (int16_t)inst;
				break;
			case LUI:
				r1 = (inst & 0xffff) << 16;
				break;
#ifdef ISA64
			case LWI:
				r1 = (long)(inst_t &)m[pc] << 32 | inst << 16;
				pc += 1 << SHAMT_I;
				CLOCK(1);
				break;
#endif
			case SEQ:
				r1 = r1 == r2;
				break;
			case SEQF:
				r1 = rf1 == rf2;
				break;
			case SNE:
				r1 = r1 != r2;
				break;
			case SNEF:
				r1 = rf1 != rf2;
				break;
			case SGT:
				r1 = r1 > r2;
				break;
			case SGTU:
				ru1 = ru1 > ru2;
				break;
			case SGTF:
				r1 = rf1 > rf2;
				break;
			case SGE:
				r1 = r1 >= r2;
				break;
			case SGEU:
				ru1 = ru1 >= ru2;
				break;
			case SGEF:
				r1 = rf1 >= rf2;
				break;
			case SLT:
				r1 = r1 < r2;
				break;
			case SLTU:
				ru1 = ru1 < ru2;
				break;
			case SLTF:
				r1 = rf1 < rf2;
				break;
			case SLE:
				r1 = r1 <= r2;
				break;
			case SLEU:
				ru1 = ru1 <= ru2;
				break;
			case SLEF:
				r1 = rf1 <= rf2;
				break;
			case CVTF:
				rf1 = (data_ft)r1;
				break;
			case CVTI:
				r1 = (data_t)rf1;
				break;
			case BRA:
				pc += (int16_t)inst << SHAMT_I;
				CLOCK(1);
				break;
			case BEQ:
				if (r1 == r2) pc += (int16_t)inst << SHAMT_I;
				CLOCK(1);
				break;
			case BEQF:
				if (rf1 == rf2) pc += (int16_t)inst << SHAMT_I;
				CLOCK(1);
				break;
			case BNE:
				if (r1 != r2) pc += (int16_t)inst << SHAMT_I;
				CLOCK(1);
				break;
			case BNEF:
				if (rf1 != rf2) pc += (int16_t)inst << SHAMT_I;
				CLOCK(1);
				break;
			case CALL:
				r[15] -= sizeof(data_t);
				(data_t &)m[r[15]] = pc;
				pc = (inst & 0xffff) << SHAMT_I;
				CLOCK(2);
				break;
			case CALLV:
				r[15] -= sizeof(data_t);
				(data_t &)m[r[15]] = pc;
				pc = (adr_t)r1;
				CLOCK(2);
				break;
			case RETI:
				intperm = true;
				// no break
			case RET:
				pc = (adr_t &)m[r[15]];
				r[15] += sizeof(data_t);
				CLOCK(2);
				break;
			case LEA:
				setadr();
				r1 = adr;
				break;
			case LD:
				setadr();
				r1 = IS_IO(adr) ? input(adr) : (int8_t &)m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs8);
				break;
			case LDU:
				setadr();
				r1 = IS_IO(adr) ? input(adr) : m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs8);
				break;
			case LD2:
				setadr();
				r1 = (int16_t &)m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs16);
				break;
			case LDU2:
				setadr();
				r1 = (uint16_t &)m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs16);
				break;
			case LD4:
				setadr();
				r1 = (int32_t &)m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs32);
				break;
			case LDU4:
				setadr();
				r1 = (uint32_t &)m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs32);
				break;
			case LD8: case LDU8:
				setadr();
				r1 = (int64_t &)m[adr];
				CLOCK(1);
				KP_TRACE_LOG(L, acs64);
				break;
			case ST:
				setadr();
				wchk(adr);
				if (IS_IO(adr)) output(adr, r1);
				else m[adr] = r1;
				KP_TRACE_LOG(S, acs8);
				break;
			case ST2:
				setadr();
				wchk(adr);
				(int16_t &)m[adr] = r1;
				KP_TRACE_LOG(S, acs16);
				break;
			case ST4:
				setadr();
				wchk(adr);
				(int32_t &)m[adr] = (int32_t)r1;
				KP_TRACE_LOG(S, acs32);
				break;
			case ST8:
				setadr();
				wchk(adr);
				(int64_t &)m[adr] = r1;
				KP_TRACE_LOG(S, acs64);
				break;
			case EI:
				intperm = true;
				break;
			case DI:
				intperm = false;
				break;
			case SLEEP:
#if KP_TRACE && DEBUG
				StopTrace();
#endif
				pc -= 1 << SHAMT_I;
				sleepf = true;
				break;
			case NOP:
				break;
			default:
				ill();
				break;
		}
		if (intreq && intperm) {
			intperm = false;
			if (sleepf) {
				sleepf = false;
				pc += 1 << SHAMT_I;
			}
			int i;
			for (i = 0; i < INT_VEC_N && !(intreq & 1 << i); i++)
				;
			intreq &= ~(1 << i);
			r[15] -= sizeof(data_t);
			(data_t &)m[r[15]] = pc;
			pc = INT_VEC + (i << 2);
			CLOCK(2);
		}
#if KP_TRACE
		int op = inst >> 24;
		if (op > 8 && op & 0xcc) {
			tracep->acsR = acs64;
			tracep->adrR = inst >> 20 & 0xf;
			tracep->dataR = r1;
		}
#if KP_TRACE > 1
		if (tracep >= tracebuf + TRACEMAX - 1) StopTrace();
#endif
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
#endif
	} while (!sleepf && CLOCK(1) < n);
	return clock - n;
}

#if KP_TRACE
#include <string>
void KP::StopTrace() {
	TraceBuffer *endp = tracep;
	int i = 0;
	FILE *fo;
	if (!(fo = fopen((std::string(getenv("HOME")) + "/Desktop/trace.txt").c_str(), "w"))) exit(1);
	do {
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
		fprintf(fo, "%7d %05X %08X ", i++, tracep->pc, (inst_t &)m[tracep->pc]);
		switch (tracep->acsR) {
			case acs32:
			fprintf(fo, "R%02d     %08X ", tracep->adrR, (uint32_t)tracep->dataR);
				break;
			case acs64:
				fprintf(fo, "R%02d     %016llX ", tracep->adrR, tracep->dataR);
				break;
		}
		switch (tracep->acsL) {
			case acs8:
				fprintf(fo, "L%06X %02X ", tracep->adrL, (uint8_t)tracep->dataL);
				break;
			case acs16:
				fprintf(fo, "L%06X %04X ", tracep->adrL, (uint16_t)tracep->dataL);
				break;
			case acs32:
				fprintf(fo, "L%06X %08X ", tracep->adrL, (uint32_t)tracep->dataL);
				break;
			case acs64:
				fprintf(fo, "L%06X %016llX ", tracep->adrL, tracep->dataL);
				break;
		}
		switch (tracep->acsS) {
			case acs8:
				fprintf(fo, "S%06X %02X ", tracep->adrS, (uint8_t)tracep->dataS);
				break;
			case acs16:
				fprintf(fo, "S%06X %04X ", tracep->adrS, (uint16_t)tracep->dataS);
				break;
			case acs32:
				fprintf(fo, "S%06X %08X ", tracep->adrS, (uint32_t)tracep->dataS);
				break;
			case acs64:
				fprintf(fo, "S%06X %016llX ", tracep->adrS, tracep->dataS);
				break;
		}
		fprintf(fo, "\n");
	} while (tracep != endp);
	fclose(fo);
	fprintf(stderr, "trace dumped.\n");
	exit(1);
}
#endif

void KP::ill() {
	pc -= 1 << SHAMT_I;
	printf("KP: Illegal instruction. PC=%04x DATA=%08x\n", pc, (inst_t &)m[pc]);
#if KP_TRACE
	StopTrace();
#endif
	exit(1);
}
