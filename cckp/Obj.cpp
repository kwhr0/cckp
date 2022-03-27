#include "Obj.h"

void Obj::line(FILE *fo, int _inst, long _imm, char reloc) {
	fprintf(fo, "T %08X %c %08lX\t", lc[0], reloc, (_inst | sz) << 24 | reg1 << 20 | reg2 << 16 | (_imm & 0xffff));
	lc[0] += INST_SIZE;
	if (reloc == 'a' || reloc == 'c') refset.insert(s);
}

void Obj::Dump(FILE *fo) {
	std::string t;
	if (inst != COMMENT && comment.length()) {
		fprintf(fo, ";%s\n", comment.c_str());
		comment.clear();
	}
	switch (inst) {
		case LI:
			if (imm >= -0x8000 && imm <= 0x7fff) {
				line(fo, LI, imm);
				fprintf(fo, "li\tr%d,%ld\n", reg1, imm);
			}
			else
#ifdef ISA64
				if (imm >= -0x80000000L && imm <= 0x7fffffffL)
#endif
			{
				line(fo, LUI, imm >> 16);
				fprintf(fo, "lui\tr%d,%ld", reg1, imm >> 16);
				if (s.length()) fprintf(fo, "\t;%s", s.c_str());
				putc('\n', fo);
				if (imm & 0xffff) {
					line(fo, ORI, imm);
					fprintf(fo, "ori\tr%d,%ld\n", reg1, imm & 0xffff);
				}
			}
#ifdef ISA64
			else {
				line(fo, LWI, imm >> 16);
				fprintf(fo, "lwi\tr%d,%ld", reg1, imm >> 16);
				if (s.length()) fprintf(fo, "\t;%s", s.c_str());
				fprintf(fo, "\nT %08X - %08lX\t;upper\n", lc[0], imm >> 32 & 0xffffffff);
				lc[0] += INST_SIZE;
				if (imm & 0xffff) {
					line(fo, ORI, imm);
					fprintf(fo, "ori\tr%d,%ld\n", reg1, imm & 0xffff);
				}
			}
#endif
			break;
		case LIL:
			if (!labels[s].first) {
				line(fo, LUI, labels[s].second >> 16, 'f');
				fprintf(fo, "lui\tr%d,high %s\n", reg1, s.c_str());
				line(fo, ORI, labels[s].second, 'g');
				fprintf(fo, "ori\tr%d,low %s\n", reg1, s.c_str());
			}
			else {
				reg2 = REGNUM_GP;
				line(fo, LEA, labels[s].second - GP_OFS, labels[s].first == 1 ? 'd' : 'e');
				fprintf(fo, "lea\tr%d,%d(r%d)", reg1, labels[s].second - GP_OFS, REGNUM_GP);
				if (s.length()) fprintf(fo, "\t;%s", s.c_str());
				putc('\n', fo);
			}
			break;
		case LEA:
			sz = 0; // in case ld/ldu -> lea
			line(fo, inst, imm, reg2 == REGNUM_GP ? flags & EXT ? 'c' : flags & DSEC ? 'd' : 'e' : '-');
			if (flags & EXT) fprintf(fo, "lea\tr%d, %s", reg1, s.c_str());
			else fprintf(fo, "lea\tr%d,%ld(r%d)", reg1, imm, reg2);
			if (s.length()) fprintf(fo, "\t;%s", s.c_str());
			putc('\n', fo);
			break;
		case LD: case LDU: case ST:
			line(fo, inst, imm, reg2 == REGNUM_GP ? flags & EXT ? 'c' : flags & DSEC ? 'd' : 'e' : '-');
			fprintf(fo, "%s.%d\t", idata[inst].s, sz == 0 ? 1 : sz == 1 ? 2 : sz == 2 ? 4 : 8);
			if (flags & EXT) fprintf(fo, "r%d, %s", reg1, s.c_str());
			else {
				fprintf(fo, "r%d,%ld(r%d)", reg1, imm, reg2);
				if (s.length()) fprintf(fo, "\t;%s", s.c_str());
			}
			fprintf(fo, "\n");
			break;
		case NEG: case COM: case NEGF: case CVTF: case CVTI:
			line(fo, inst);
			fprintf(fo, "%s\tr%d\n", idata[inst].s, reg1);
			break;
		case CALLV:
			line(fo, CALLV);
			fprintf(fo, "%s\tr%d\n", idata[inst].s, reg1);
			break;
		case MOV: case AND: case OR: case XOR:
		case ADD: case SUB: case MUL: case DIV: case REM: case MULU: case DIVU: case REMU:
		case SEQ: case SNE: case SLT: case SLE: case SLTU: case SLEU: case SGT: case SGE: case SGTU: case SGEU:
		case SLLV: case SRAV: case SRLV:
		case ADDF: case SUBF: case MULF: case DIVF:
		case SEQF: case SNEF: case SLTF: case SLEF: case SGTF: case SGEF:
			line(fo, inst);
			fprintf(fo, "%s\tr%d,r%d", idata[inst].s, reg1, reg2);
			if (s.length()) fprintf(fo, "\t;%s", s.c_str());
			putc('\n', fo);
			break;
		case ADDI: case SLL: case SRA: case SRL: case ANDI: case ORI: case XORI:
			line(fo, inst, imm);
			fprintf(fo, "%s\tr%d,%ld\n", idata[inst].s, reg1, imm);
			break;
		case BRA:
			t = s;
			if (p1) t += std::to_string(p1);
			if (suffix) t += "_" + std::to_string(suffix);
			line(fo, inst, ((labels[t].second - lc[0] - 1) >> 2));
			fprintf(fo, "bra\t%s\n", t.c_str());
			break;
		case BEQ: case BNE: case BEQF: case BNEF:
			t = s;
			if (p3) t += std::to_string(p3);
			if (p4) t += "_" + std::to_string(p4);
			if (suffix) t += "_" + std::to_string(suffix);
			line(fo, inst, ((labels[t].second - lc[0] - 1) >> 2));
			fprintf(fo, "%s\tr%d,r%d,%s\n", idata[inst].s, reg1, reg2, t.c_str());
			break;
		case CALL:
			line(fo, inst, labels[s].second >> 2, G::funcs.count(s) && G::funcs[s]->attr._static ? 'b' : 'a');
			fprintf(fo, "call\t%s\n", s.c_str());
			break;
		case RET: case RETI: case EI: case DI: case SLEEP: case NOP:
			line(fo, inst);
			fprintf(fo, "%s\n", idata[inst].s);
			break;
		case LABEL:
			t = s;
			if (p1) t += std::to_string(p1);
			if (p2) t += "_" + std::to_string(p2);
			if (suffix) t += "_" + std::to_string(suffix);
			fprintf(fo, "%s:\n", t.c_str());
			labels[t] = std::pair<int, int>(section, lc[section]);
			break;
		case DEF:
			fprintf(fo, "D %08X - ", lc[1]);
			if (p1 == 1) fprintf(fo, "%02lX      ", imm & 0xff);
			else if (p1 == 2) fprintf(fo, "%04lX    ", imm & 0xffff);
			else fprintf(fo, "%08lX", imm & 0xffffffff);
			fprintf(fo, "\t.def.%d\t", p1);
			if (p1 == 8) fprintf(fo, "%ld", imm);
			if (s.length()) fprintf(fo, "\t;%s", s.c_str());
			if (p1 == 8) fprintf(fo, "\nD %08X - %08lX\t;upper\n", lc[1] + INST_SIZE, imm >> 32 & 0xffffffff);
			else if (s.length()) fprintf(fo, "%s%ld\n", s.c_str(), imm);
			else fprintf(fo, "%ld\n", imm);
			lc[1] += p1;
			break;
		case SPACE:
			lc[2] = p1;
			labels[s] = std::pair<int, int>(2, lc[2]);
			if (s.length()) fprintf(fo, "%s:\n", s.c_str());
			fprintf(fo, "B %08X - \t.space.%d\toffset=%d\n", lc[2], (int)p2, p1);
			lc[2] += p2;
			break;
		case DATA:
			t = s;
			if (p2) t += std::to_string(p2);
			section = 1;
			lc[1] = p1;
			labels[t] = std::pair<int, int>(1, lc[1]);
			fprintf(fo, "\t.data\toffset=%d\n", p1);
			if (t.length()) fprintf(fo, "%s:\n", t.c_str());
			break;
		case ENDDATA:
			section = 0;
			fprintf(fo, "\t.enddata\n");
			break;
		case COMMENT:
			comment = s;
			break;
		default:
			break;
	}
}

void Obj::ReplaceArg(int save, int ofs, int regnum) {
	for (auto it = begin(code) + save; it != end(code); it++) {
		if (it->inst == ADDI && it->reg1 == REGNUM_SP) ofs -= it->imm;
		else if (it->reg2 == REGNUM_SP && it->imm == ofs && it->flags & ARG) {
			if (it->IsLoad()) it->reg2 = regnum;
			else if (it->inst == ST) {
				it->reg2 = it->reg1;
				it->reg1 = regnum;
			}
			it->inst = MOV;
			it->sz = 0;
		}
	}
}

void Obj::Optimize() {
	std::vector<Obj>::iterator i, j;
	int f, r;
	bool used;
	// summarize addi
	for (i = begin(code); i != end(code); i++) {
		if (i->inst == ADDI) {
			r = i->reg1;
			for (j = i + 1; j != end(code);)
				if (j->inst == ADDI && j->reg1 == r) {
					i->imm += j->imm;
					j = code.erase(j);
				}
				else break;
		}
	}
	// offset pointer instead of addi
	for (i = begin(code); i != end(code);) {
		if (i->inst == ADDI && (r = i->reg1) <= REGNUM_END) {
			int ofs = (int)i->imm;
			used = false;
			for (j = i + 1; j != end(code); j++) {
				if ((j->IsLoad() || j->inst == ST) && j->reg2 == r) {
					used = true;
					j->imm += ofs;
					j->s = i->s;
				}
				if (idata[j->inst].flags & (W1 | M1) && j->reg1 == r) break;
			}
			i = used ? code.erase(i) : i + 1;
		}
		else i++;
	}
	// delete not used result
	for (i = begin(code); i != end(code);) {
		f = idata[i->inst].flags;
		if ((f & (W1 | M1)) && (r = i->reg1) <= REGNUM_END && (!(f & R2) || i->reg2 <= REGNUM_END)) {
			used = false;
			for (j = i + 1; j != end(code); j++) {
				f = idata[j->inst].flags;
				if ((f & R1 && j->reg1 == r) || (f & R2 && j->reg2 == r) || f & B) {
					used = true;
					break;
				}
				if (f & W1 && r == j->reg1) break;
			}
			i = used ? i + 1 : code.erase(i);
		}
		else i++;
	}
}

void Obj::MakeInline(std::string &name, size_t save) {
	if (save >= 3 && code[save - 1].inst == COMMENT && code[save - 2].inst == LABEL && code[save - 3].inst == COMMENT)
		code[save - 3].s = "";
	for (auto it = begin(code) + save; it != end(code); it++)
		if ((it->IsAccess()) && it->reg2 == REGNUM_SP) it->flags |= FIX;
	inlinecode[name] = std::vector<Obj>(begin(code) + save, end(code));
	code.resize(save);
}

// before calling, reserve regnum + 1
// size must be multiple of UNIT_SIZE
void Obj::_memcpy(int regnum, int size) {
	if (size < 4 * UNIT_SIZE)
		for (int i = 0; i < size; i += UNIT_SIZE) {
			code.emplace_back(LD, regnum, i, regnum - 1, UNIT_SIZE);
			code.emplace_back(ST, regnum, i, regnum - 2, UNIT_SIZE);
		}
	else {
		code.emplace_back(MOV, regnum, regnum - 2);
		code.emplace_back(ADDI, regnum, size & ~(UNIT_SIZE - 1));
		code.emplace_back(LABEL, "memcpy",  ++labelNum);
		code.emplace_back(LD, regnum + 1, 0, regnum - 1, UNIT_SIZE);
		code.emplace_back(ADDI, regnum - 1, UNIT_SIZE);
		code.emplace_back(ST, regnum + 1, 0, regnum - 2, UNIT_SIZE);
		code.emplace_back(ADDI, regnum - 2, UNIT_SIZE);
		code.emplace_back(BNE, "memcpy", regnum, regnum - 2, labelNum);
	}
}

void Obj::crt0(FILE *fo) {
	StaticInit();
	code.emplace_back(BRA, "_reset_");
	for (int i = 0; i < INT_VEC_N; i++) code.emplace_back(RETI); // reserve for interrupt
	code.emplace_back(LABEL, "_reset_");
	code.emplace_back(LI, REGNUM_SP, MEM_AMOUNT);
	code.emplace_back(LI, REGNUM_GP, ORG_DATA + GP_OFS);
	code.emplace_back(LEA, REGNUM_BASE, 0, REGNUM_GP);
	code.emplace_back(LI, REGNUM_BASE + 1, 0);
	code.emplace_back(LI, REGNUM_BASE + 2, 0);
	code.emplace_back(BRA, "bss_clear_tail");
	code.emplace_back(LABEL, "bss_clear_loop");
	code.emplace_back(ST, REGNUM_BASE + 1, 0, REGNUM_BASE, UNIT_SIZE);
	code.emplace_back(ADDI, REGNUM_BASE, UNIT_SIZE);
	code.emplace_back(ADDI, REGNUM_BASE + 2, -1);
	code.emplace_back(LABEL, "bss_clear_tail");
	code.emplace_back(BNE, "bss_clear_loop", REGNUM_BASE + 2, REGNUM_ZERO);
	code.emplace_back(EI);
	code.emplace_back(CALL, "main");
	code.emplace_back(LABEL, "sleep_loop");
	code.emplace_back(SLEEP);
	code.emplace_back(BRA, "sleep_loop");
	code.emplace_back(LABEL, "sleep");
	code.emplace_back(SLEEP);
	code.emplace_back(RET);
	code.emplace_back(LABEL, "memset_align");
	code.emplace_back(LI, REGNUM_RET, ~(UNIT_SIZE - 1));
	code.emplace_back(AND, REGNUM_ARG + 2, REGNUM_RET);
	code.emplace_back(ADD, REGNUM_ARG + 2, REGNUM_ARG); // limit
	code.emplace_back(BRA, "memset_align_while_tail");
	code.emplace_back(LABEL, "memset_align_while_top");
	code.emplace_back(ST, REGNUM_ARG + 1, 0, REGNUM_ARG, UNIT_SIZE);
	code.emplace_back(ADDI, REGNUM_ARG, UNIT_SIZE);
	code.emplace_back(LABEL, "memset_align_while_tail");
	code.emplace_back(BNE, "memset_align_while_top", REGNUM_ARG, REGNUM_ARG + 2);
	code.emplace_back(RET);
	code.emplace_back(LABEL, "memcpy_align");
	code.emplace_back(LI, REGNUM_RET, ~(UNIT_SIZE - 1));
	code.emplace_back(AND, REGNUM_ARG + 2, REGNUM_RET);
	code.emplace_back(ADD, REGNUM_ARG + 2, REGNUM_ARG); // limit
	code.emplace_back(BRA, "memcpy_align_while_tail");
	code.emplace_back(LABEL, "memcpy_align_while_top");
	code.emplace_back(LD, REGNUM_RET, 0, REGNUM_ARG + 1, UNIT_SIZE);
	code.emplace_back(ADDI, REGNUM_ARG + 1, UNIT_SIZE);
	code.emplace_back(ST, REGNUM_RET, 0, REGNUM_ARG, UNIT_SIZE);
	code.emplace_back(ADDI, REGNUM_ARG, UNIT_SIZE);
	code.emplace_back(LABEL, "memcpy_align_while_tail");
	code.emplace_back(BNE, "memcpy_align_while_top", REGNUM_ARG, REGNUM_ARG + 2);
	code.emplace_back(RET);
	G::funcs["sleep"] = new Type(Type::FUNC);
	G::funcs["memset_align"] = new Type(Type::FUNC);
	G::funcs["memcpy_align"] = new Type(Type::FUNC);
	DumpAll(fo);
}

void Obj::DumpAll(FILE *fo) {
	Optimize();
	lc[0] = lc[1] = lc[2] = 0;
	section = 0;
	for (Obj &o : code) o.Dump(); // rescan because code may shrink by optimization
	Align(lc[1], UNIT_SIZE);
	Align(lc[2], UNIT_SIZE);
	fprintf(fo, "#SECTION LENGTH\nT %08X\nD %08X\nB %08X\n#GLOBAL\n", lc[0], lc[1], lc[2]);
	for (auto [name, type] : G::funcs) {
		if (!type->attr._static && !type->attr._extern && !type->attr._inline)
			fprintf(fo, "T %08X %s\n", labels[name].second, name.c_str());
		if (!type->attr._extern) refset.erase(name);
	}
	for (auto [name, var] : G::scopeStack.front().vars) {
		if (var.defined && !var.type->attr._static)
			fprintf(fo, "%c %08X %s\n", var.datasection ? 'D' : 'B', var.ofs, name.c_str());
		if (var.defined) refset.erase(name);
	}
	fprintf(fo, "#EXTERN\n");
	for (auto &s : refset) fprintf(fo, "%s\n", s.c_str());
	fprintf(fo, "#OBJECT\n");
	lc[0] = lc[1] = lc[2] = 0;
	section = 0;
	for (Obj &o : code) o.Dump(fo);
}
