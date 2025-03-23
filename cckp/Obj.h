#include "types.h"
#include <set>

struct Obj {
	enum Flag { ARG = 1, FIX = 2, EXT = 4, DSEC = 8 };
	enum RW { R1 = 1, W1 = 2, M1 = 4, R2 = 8, B = 0x10, I = 0x20 };
	enum Inst {
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
/*30*/	ST, ST2, ST4, ST8, _34, _35, _36, _37,
		LD, LD2, LD4, LD8, LDU, LDU2, LDU4, LDU8,
		// 01oooooo rrrrrrrr xxxxxxxx xxxxxxxx
/*40*/	ADD, SUB, MUL, DIV, REM, MULU, DIVU, REMU,
/*48*/	ADDF, SUBF, MULF, DIVF, AND, OR, XOR, MOV,
/*50*/	SLLV, _31, SRLV, SRAV, SEQ, SNE, SEQF, SNEF,
/*58*/	SLT, SLE, SLTU, SLEU, SGT, SGE, SGTU, SGEU,
/*60*/	SLTF, SLEF, SGTF, SGEF,
		// psuedo inst.
		SUBI, LIL, LABEL, DATA, ENDDATA, DEF, SPACE, COMMENT
	};
	static inline struct {
		const char *s;
		int flags;
	} idata[] = {
		{ "nop", 0 },  { "sleep", 0 }, { "ei", 0 }, { "di", 0 }, { "ret", 0 }, { "reti", 0 },
		{ "*", 0 }, { "*", 0 },
		{ "callv", R1 }, { "com", M1 }, { "neg", M1 }, { "negf", M1 }, { "cvtf", M1 }, { "cvti", M1 },
		{ "*", 0 }, { "*", 0 },
		{ "bra", B }, { "call", 0 }, { "*", 0 }, { "*", 0 },
		{ "addi", M1 | I }, { "*", 0 }, { "lui", W1 | I }, { "lwi", W1 | I },
		{ "andi", M1 | I }, { "ori", M1 | I }, { "xori", M1 | I }, { "li", W1 | I },
		{ "sll", M1 | I }, { "*", 0 }, { "srl", M1 | I }, { "sra", M1 | I },
		{ "beq", R1 | R2 | B }, { "bne", R1 | R2 | B }, { "beqf", R1 | R2 | B }, { "bnef", R1 | R2 | B },
		{ "*", 0 }, { "*", 0 }, { "*", 0 }, { "*", 0 },
		{ "*", 0 }, { "*", 0 }, { "*", 0 }, { "*", 0 }, { "*", 0 }, { "*", 0 }, { "*", 0 }, { "lea", W1 | R2 | I },
		{ "st", R1 | R2 | I }, { "st2", R1 | R2 | I }, { "st4", R1 | R2 | I }, { "st8", R1 | R2 | I },
		{ "*", 0 }, { "*", 0 }, { "*", 0 }, { "*", 0 },
		{ "ld", W1 | R2 | I }, { "ld2", W1 | R2 | I }, { "ld4", W1 | R2 | I }, { "ld8", W1 | R2 | I },
		{ "ldu", W1 | R2 | I }, { "ldu2", W1 | R2 | I }, { "ldu4", W1 | R2 | I }, { "ldu8", W1 | R2 | I },
		{ "add", M1 | R2 }, { "sub", M1 | R2 }, { "mul", M1 | R2 }, { "div", M1 | R2 }, { "rem", M1 | R2 },
		{ "mulu", M1 | R2 }, { "divu", M1 | R2 }, { "remu", M1 | R2 },
		{ "addf", M1 | R2 }, { "subf", M1 | R2 }, { "mulf", M1 | R2 }, { "divf", M1 | R2 },
		{ "and", M1 | R2 }, { "or", M1 | R2 }, { "xor", M1 | R2 }, { "mov", W1 | R2 },
		{ "sllv", M1 | R2 }, { "*", 0 }, { "srlv", M1 | R2 }, { "srav", M1 | R2 },
		{ "seq", M1 | R2 }, { "sne", M1 | R2 }, { "seqf", M1 | R2 }, { "snef", M1 | R2 },
		{ "slt", M1 | R2 }, { "sle", M1 | R2 }, { "sltu", M1 | R2 }, { "sleu", M1 | R2 },
		{ "sgt", M1 | R2 }, { "sge", M1 | R2 }, { "sgtu", M1 | R2 }, { "sgeu", M1 | R2 },
		{ "sltf", M1 | R2 }, { "slef", M1 | R2 }, { "sgtf", M1 | R2 }, { "sgef", M1 | R2 },
		{ "(subi)", M1 | I }, { "(li)", W1 | I }, { "(label)", 0 }, { ".data", 0 }, { ".enddata", 0 },
		{ ".def", I }, { ".space", 0 }, { "(comment)", 0 },
	};
	Obj() {}
	Obj(Inst _inst, int _p1, int _p2 = 0, int _p3 = 0, int _p4 = 0, std::string _s = "", int _flags = 0)
		: inst(_inst), p1(_p1), p2(_p2), p3(_p3), p4(_p4), s(_s), flags(_flags) {
		Init();
		if (inst == SUBI) {
			inst = ADDI;
			imm = -imm;
		}
		if (IsAccess()) {
			sz = p4 == 1 ? 0 : p4 == 2 ? 1 : p4 == 4 ? 2 : 3;
			reg2 = p3;
		}
		Dump();
	}
	// branch/call/return/label/literal
	Obj(Inst _inst, std::string _s = "", int _p1 = 0, long _p2 = 0, int _p3 = 0, int _p4 = 0)
		: inst(_inst), p1(_p1), p2(_p2), p3(_p3), p4(_p4), s(_s), flags(0) {
		Init();
		Dump();
	}
	Obj(std::string _s) : inst(COMMENT), s(_s) {
		Dump();
	}
	void Init() {
		int f = idata[inst].flags;
		reg1 = f & (R1 | W1 | M1) ? p1 : 0;
		reg2 = f & R2 ? (int)p2 : 0;
		imm = f & I ? p2 : 0;
		sz = 0;
		suffix = 0;
	}
	void line(FILE *fo, int _inst, long _imm = 0, char reloc = '-');
	void Dump(FILE *fo = fnull);
	bool IsLoad() const { return inst == LD || inst == LDU; }
	bool IsAccess() const { return inst == LEA || IsLoad() || inst == ST; }
	Inst inst;
	int p1, p3, p4, flags, reg1, reg2, sz, suffix;
	long p2, imm;
	std::string s;
	template<typename F> static void AdjustOffset(size_t save, F func) {
		for (auto it = begin(code) + save; it != end(code); it++)
			if ((it->IsAccess()) && it->reg2 == REGNUM_SP && !(it->flags & FIX))
				it->imm += func((it->flags & ARG) != 0);
	}
	static void MakeInline(std::string &name, size_t save);
	static void ReplaceArg(int save, int ofs, int regnum);
	static void Optimize();
	static void _memcpy(int regnum, int size);
	static void crt0(FILE *fo);
	static void DumpAll(FILE *fo);
	static void StaticInit() {
		if (!fnull) {
			fnull = fopen("/dev/null", "w");
			if (!fnull) exit(1);
		}
		code.clear();
		section = labelNum = 0;
		lc[0] = lc[1] = lc[2] = 0;
		comment.clear();
		labels.clear();
		inlinecode.clear();
		refset.clear();
	}
	static inline FILE *fnull;
	static inline std::vector<Obj> code;
	static inline int section, labelNum;
	static inline int lc[3];
	static inline std::string comment;
	static inline std::map<std::string, std::pair<int, int>> labels;
	static inline std::map<std::string, std::vector<Obj>> inlinecode;
	static inline std::set<std::string> refset;
};
