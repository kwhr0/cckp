#include "Link.h"
#include "Error.h"
#include "Obj.h"
#include <unistd.h>

#define CRT0		"/tmp/crt0.o"
#define CRT0_START	((4 + INT_VEC_N) * INST_SIZE)
#define CRT0_LENGTH	((6 + INT_VEC_N) * INST_SIZE)
#define LIST		"a.lst"
#define ADR			"a.adr"
#define LIBPATHEXT	".lp"

static FILE *fo, *fa, *lst;
static std::map<std::string, std::string> libref;
static std::vector<std::pair<FILE *, std::string>> files;

void ReadLibPath(std::string path) {
	char s[256], name[256], opath[256];
	path += LIBPATHEXT;
	FILE *flb = fopen(path.c_str(), "r");
	if (!flb) Fatal(FATAL_OPEN, path.c_str());
	while (fgets(s, sizeof(s), flb))
		if (sscanf(s, "%255s%255s", name, opath) == 2) libref[name] = opath;
	fclose(flb);
}

static void writeSRecord(FILE *fo, std::vector<uint8_t> &bin, int start, int end) {
	int i, sum = 0;
	for (i = start; i < end; i++) {
		if (!(i & 15)) {
			int len = int((4 + std::min(16, end - i)) << 1);
			fprintf(fo, "S2%02X%06X", len, i);
			sum = len + i + (i >> 8) + (i >> 16);
		}
		fprintf(fo, "%02X", bin[i]);
		sum += bin[i];
		if ((i & 15) == 15) fprintf(fo, "%02X\n", ~sum & 0xff);
	}
	if (i & 15) fprintf(fo, "%02X\n", ~sum & 0xff);
}

void Link(int argc, char *argv[], std::string &archive) {
	fo = fa = lst = NULL;
	try {
		G::funcs.clear();
		G::scopeStack.clear();
		G::scopeStack.emplace_back();
		files.clear();
		fo = fopen(CRT0, "w");
		if (!fo) Fatal(FATAL_WRITE, CRT0);
		Obj::crt0(fo);
		fclose(fo);
		std::string::size_type pos;
		files.emplace_back(fopen(CRT0, "r"), "crt.o");
		if (!files.back().first) Fatal(FATAL_OPEN, CRT0);
		int i;
		for (i = optind; i < argc; i++) {
			std::string s1 = argv[i];
			pos = s1.rfind(".c");
			if (pos != std::string::npos) s1[pos + 1] = 'o';
			files.emplace_back(fopen(s1.c_str(), "r"), s1);
			if (!files.back().first) Fatal(FATAL_OPEN, s1.c_str());
		}
		if (!archive.empty()) {
			archive += LIBPATHEXT;
			fa = fopen(archive.c_str(), "w");
			if (!fa) Fatal(FATAL_WRITE, archive.c_str());
		}
		std::set<std::string> undefs, multidefs;
		int adr;
		struct Offset {
			int o[3];
		} ofs;
		std::vector<Offset> ofsAry;
		std::map<std::string, int> labels;
		int interrupt[10] = {};
		char s[256], sec[256];
		for (bool append = true; append;) {
			ofsAry.clear();
			ofs = { 0, ORG_DATA, 0 };
			for (auto &fi : files) {
				ofsAry.push_back(ofs);
				rewind(fi.first);
				while (fgets(s, sizeof(s), fi.first)) {
					if (sscanf(s, "%255s%x", sec, &adr) == 2)
						(*sec == 'T' ? ofs.o[0] : *sec == 'D' ? ofs.o[1] : ofs.o[2]) += adr;
					else if (strstr(s, "#G")) break;
				}
			}
			ofsAry.push_back(ofs);
			for (auto &o : ofsAry) o.o[2] += ofs.o[1];
			labels.clear();
			i = 0;
			for (auto &fi : files) {
				ofs = ofsAry[i++];
				while (fgets(s, sizeof(s), fi.first)) {
					char name[256];
					if (sscanf(s, "%255s%x%255s", sec, &adr, name) == 3) {
						if (labels.count(name)) multidefs.insert(name);
						int x = *sec == 'T' ? 0 : *sec == 'D' ? 1 : 2;
						labels[name] = ofs.o[x] + adr;
						if (fa && strcmp(name, "sleep") && fi.second.length()) fprintf(fa, "%s %s\n", name, fi.second.c_str());
						if ((x = GetInterruptNum(name))) interrupt[x - 1] = labels[name];
					}
					else if (strstr(s, "#E")) break;
				}
			}
			if (fa) throw 1;
			std::map<std::string, std::string> externs;
			for (auto &fi : files)
				while (fgets(s, sizeof(s), fi.first) && !strstr(s, "#O")) {
					s[strlen(s) - 1] = 0;
					externs[s] = fi.second;
				}
			for (auto &p : labels) externs.erase(p.first);
			std::set<std::string> ftr;
			for (auto &p : externs)
				if (libref.count(p.first)) ftr.insert(libref[p.first]);
				else undefs.insert(p.first);
			append = false;
			for (auto &s : ftr) {
				pos = s.rfind("/");
				files.emplace_back(fopen(s.c_str(), "r"), pos != std::string::npos ? s.substr(pos + 1) : s);
				if (!files.back().first) Fatal(FATAL_OPEN, libref[s].c_str());
				append = true;
			}
		}
		for (auto &s : undefs) Error(ERR_LINK_UNDEF, s.c_str());
		for (auto &s : multidefs) Error(ERR_LINK_MULTI, s.c_str());
		if (ErrorCount()) throw 1;
		printf("TEXT: %05X-%05X\n", ofsAry.front().o[0], ofsAry.back().o[0]);
		printf("DATA: %05X-%05X\n", ofsAry.front().o[1], ofsAry.back().o[1]);
		printf(" BSS: %05X-%05X\n", ofsAry.front().o[2], ofsAry.back().o[2]);
		lst = fopen(LIST, "w");
		if (!lst) Fatal(FATAL_WRITE, LIST);
		std::vector<uint8_t> bin;
		bin.reserve(ofsAry.back().o[1]);
		std::vector<std::tuple<int, std::string, std::string>> adrinf;
		i = 0;
		for (auto &fi : files) {
			ofs = ofsAry[i++];
			std::string entry;
			while (fgets(s, sizeof(s), fi.first)) {
				char reloc[256], data[256], opecode[256], operand1[256], operand2[256];
				int n = sscanf(s, "%255s%x%255s%255s%255s%255s%255s", sec, &adr, reloc, data, opecode, operand1, operand2);
				if (n >= 4 && (*sec == 'T' || *sec == 'D')) {
					adr += (*sec == 'T' ? ofs.o[0] : ofs.o[1]);
					int d, bytes = int(strlen(data) >> 1);
					sscanf(data, "%x", &d);
					switch (*reloc) {
						case 'a':
							if (n >= 6 && !labels.count(operand1)) Error(ERR_LINK_UNDEF, operand1);
							d = (d & ~0xffff) | labels[operand1] >> 2;
							break;
						case 'b':
							d = (d & ~0xffff) | (d & 0xffff) + (ofs.o[0] >> 2);
							break;
						case 'c':
							if (n >= 7 && !labels.count(operand2)) Error(ERR_LINK_UNDEF, operand2);
							d = (d & ~0xffff) | (labels[operand2] - ORG_DATA - GP_OFS & 0xffff);
							break;
						case 'd': case 'e':
							d = (d & ~0xffff) | (d & 0xffff) + ofs.o[*reloc - 'd' + 1] - ORG_DATA;
							break;
						case 'f':
							d = (d & ~0xffff) | ((d & 0xffff) + ofs.o[0]) >> 16;
							break;
						case 'g':
							d = (d & ~0xffff) | (d & 0xffff) + ofs.o[0];
							break;
					}
					char *mn = s + 22;
					char t[32];
					if (*sec == 'T') {
						if (adr == CRT0_START)
							d = (d & ~0xffff) | (ofsAry.front().o[2] - GP_OFS & 0xffff);
						else if (adr == CRT0_LENGTH)
							d = (d & ~0xffff) | ((ofsAry.back().o[2] - ofsAry.front().o[2]) / UNIT_SIZE & 0xffff);
						else if (adr >= INT_VEC && adr < INT_VEC + INT_VEC_N * INST_SIZE) {
							int a = interrupt[(adr - INT_VEC) / INST_SIZE];
							if (a) {
								d = Obj::BRA << 24 | (a - adr - 4) >> 2;
								sprintf(mn = t, "bra interrupt%d\n", adr >> 2);
							}
						}
					}
					if (entry.length()) {
						adrinf.emplace_back(adr, entry, fi.second);
						entry = "";
					}
					fprintf(lst, "%08X ", adr);
					fprintf(lst, bytes == 1 ? "%02X      " : bytes == 2 ? "%04X    " : "%08X", d);
					fprintf(lst, " %s", mn);
					while (bytes--) {
						bin[adr++] = d;
						d >>= 8;
					}
				}
				else {
					fprintf(lst, "%s", s);
					if (!strncmp(s, ";FUNC", 5)) {
						s[strlen(s) - 1] = 0;
						entry = s + 6;
					}
				}
			}
		}
		FILE *fadr = fopen(ADR, "w");
		for (auto &p : adrinf) fprintf(fadr, "%05x %s (%s)\n", std::get<0>(p), std::get<1>(p).c_str(), std::get<2>(p).c_str());
		fclose(fadr);
		if (archive.empty()) {
			fo = fopen(A_OUT, "w");
			if (!fo) Fatal(FATAL_WRITE, A_OUT);
		}
		writeSRecord(fo, bin, ofsAry.front().o[0], ofsAry.back().o[0]);
		writeSRecord(fo, bin, ofsAry.front().o[1], ofsAry.back().o[1]);
		fprintf(fo, "S804000000FC\n");
	}
	catch (...) {}
	remove(CRT0);
	if (fo) fclose(fo);
	if (fa) fclose(fa);
	if (lst) fclose(lst);
	for (auto &p : files) if (p.first) fclose(p.first);
}
