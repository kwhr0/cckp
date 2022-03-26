#include "File.h"
#include "Error.h"
#include "Obj.h"
#include "Prepro.h"

#define kUngetMargin		0x10000

struct Buf {
	Buf() : buf(nullptr), rp(nullptr), linen(0) {}
	Buf(int size) : buf(new char[size]), rp(buf + kUngetMargin), linen(0) {}
	std::string path;
	char *buf, *rp;
	int linen;
};

char (*_getc)();
char (*getch)();

static char *start, *filerp;
static int lineNum, lineNumLast;
static std::string *dst;
static std::vector<Buf *> fileStack;

void FileClear() {
	fileStack.clear();
}

std::string getCurPath() {
	return fileStack.empty() ? "" : fileStack.back()->path;
}

void AdjustLineNum(char c) {
	if (c == '\n') lineNumLast++;
	for (; lineNumLast < lineNum; lineNumLast++) *dst += '\n';
}

char *ErrorInfo(int *pos) {
	if (fileStack.empty()) return nullptr;
	for (Buf *buf : fileStack)
		if (buf != fileStack.back()) fprintf(stderr, "included from %s\n", buf->path.c_str());
	fprintf(stderr, "%s(%d): ", fileStack.back()->path.c_str(), lineNum);
	char *p;
	for (p = filerp - 1; p >= start && *p != '\n'; p--)
		;
	p++;
	if (pos) *pos = int(filerp - p - 1);
	return p;
}

static void printLine() {
	char s[256];
	sprintf(s, "#line %d \"%.200s\"\n", lineNum, fileStack.back()->path.c_str());
	*dst += s;
}

static char *Open(const char *path) {
	FILE *fi;
	if (!(fi = fopen(path, "rb"))) Fatal(FATAL_OPEN, path);
	fseek(fi, 0, SEEK_END);
	int size = (int)ftell(fi);
	rewind(fi);
	Buf *p = new Buf(size + kUngetMargin + 1);
	char *top = p->buf + kUngetMargin, *sp = top, *dp = top;
	fread(top, size, 1, fi);
	fclose(fi);
	int cf = 0;
	for (; sp < top + size; sp++)
		if (char c = *sp; c != 13)
			switch (cf) {
				case 0:
					if (c == '/')
						if (sp[1] == '/') cf = -1;
						else if (sp[1] == '*') cf++;
						else *dp++ = c;
					else *dp++ = c;
					break;
				case -1:
					if (c != '\n') break;
					cf = 0;
					*dp++ = '\n';
					break;
				default:
					if (c == '\n') *dp++ = '\n';
					if (c != '*' || sp[1] != '/') break;
					if (!--cf) *dp++ = ' ';
					sp++;
					break;
			}
	if (cf) Error(ERR_COMMENT, path);
	*dp = 0;
	fileStack.push_back(p);
	return top;
}

void PushInc(std::string &pathname) {
	fileStack.back()->rp = filerp;
	fileStack.back()->linen = lineNum;
	filerp = Open(pathname.c_str());
	start = fileStack.back()->buf;
	fileStack.back()->path = pathname;
	lineNum = lineNumLast = 1;
	printLine();
}

static void PopInc() {
	fileStack.pop_back();
	filerp = fileStack.back()->rp;
	start = fileStack.back()->buf;
	lineNum = lineNumLast = fileStack.back()->linen;
	printLine();
}

static long oct(char c) {
	long v = 0;
	for (; c >= '0' && c <= '7'; c = *filerp++) v = (v << 3) + c - '0';
	*--filerp = c;
	return v;
}

static long decimal(char c) {
	long v = 0;
	for (; isdigit(c); c = *filerp++) v = 10 * v + c - '0';
	*--filerp = c;
	return v;
}

static long hex(char c) {
	long v = 0;
	for (; isdigit(c) || (toupper(c) >= 'A' && toupper(c) <= 'F'); c = *filerp++)
		v = (v << 4) + (isdigit(c) ? c - '0' : toupper(c) - 'A' + 10);
	*--filerp = c;
	return v;
}

static void comment() {
	std::string s;
	char *p = filerp;
	while (*p && (*p == '\t' || *p == ' ')) p++;
	while (*p && *p != '\n' && *p != '#') s += *p++;
	if (s.length()) {
		char s0[16];
		sprintf(s0, "#L%d\t", lineNum);
		Obj::code.emplace_back(s0 + s);
	}
}

char getcsub() {
	char c;
	while (!(c = *filerp++)) {
		filerp--;
		if (fileStack.size() <= 1) return 0;
		PopInc();
	}
	return c;
}

static char getc_compile() {
	char c;
	while ((c = getcsub()) == '#') {
		if (getword() != "line") Fatal(FATAL_PREPRO);
		lineNum = lineNumLast = (int)decimal(getch());
		needch('\"');
		std::string s;
		while (*filerp != '\"') s += *filerp++;
		fileStack.back()->path = s;
		while (*filerp++ != '\n')
			;
		comment();
	}
	if (c == '\n') {
		lineNum++;
		comment();
	}
	return c;
}

static char getc_prepro() {
	char c, c2;
	while ((c = getcsub()) == '\\')
		if ((c2 = getcsub()) == '\n') lineNum++;
		else {
			if (c2) *--filerp = c2;
			return '\\';
		}
	if (c == '\n') lineNum++;
	return c;
}

void _ungetc(char c) {
	if (c) {
		if (filerp > start) *--filerp = c;
		else Fatal(FATAL_INTERNAL);
		if (c == '\n') lineNum--;
	}
}

char chkch() {
	char c = getch();
	_ungetc(c);
	return c;
}

void needch(char c) {
	if (getch() != c) Error(ERR_NEEDS_CH, c);
}

void assertch(char c1, char c2) {
	if (c1 != c2) Error(ERR_NEEDS_CH, c2);
}

std::string _gets() {
	std::string r;
	char c = *filerp++;
	while (c && c != '\"') {
		r += c == '\\' ? escape() : c;
		c = *filerp++;
	}
	*--filerp = c;
	return r;
}

void ungets(std::string &s) {
	for (int i = (int)s.length() - 1; i >= 0; i--) _ungetc(s[i]);
}

std::string getword() {
	std::string r;
	std::set<std::string> expanded;
	bool f;
	do {
		f = false;
		r.clear();
		if (char c = getch()) {
			if (isalpha(c) || c == '_') {
				r += c;
				for (c = _getc(); isalnum(c) || c == '_'; c = _getc()) r += c;
			}
			_ungetc(c);
			if (!expanded.count(r)) f = ExpandMacro(r);
			if (f) expanded.insert(r);
		}
	} while (f);
	return r;
}

static bool getfloat(FLOAT &v) {
	bool point = false;
	char *p = filerp;
	char c = *p++;
	for (; isdigit(c) || c == '.'; c = *p++)
		if (c == '.') {
			if (point) return false;
			point = true;
		}
	if (toupper(c) == 'E') {
		c = *p++;
		if (c == '+' || c == '-') c = *p++;
		while (isdigit(c)) c = *p++;
	}
	else if (!point) return false;
	if (toupper(c) == 'F' || toupper(c) == 'L') p++;
	if (sscanf(filerp, FLOAT_FMT, &v) != 1) return false;
	filerp = p - 1;
	return true;
}

bool getvalue(long &vi, FLOAT &vf) {
	if (getfloat(vf)) {
		vi = vf;
		return true; // value is float
	}
	char c = *filerp++;
	if (c == '0') vf = vi = toupper(c = *filerp++) == 'X' ? hex(*filerp++) : oct(filerp[-1]);
	else if (c >= '1' && c <= '9') vf = vi = decimal(c);
	else Error(ERR_NEEDS_VALUE);
	while (toupper(c = getch()) == 'L' || toupper(c) == 'U')
		;
	_ungetc(c);
	return false; // value is int
}

char escape() {
	char c = *filerp++;
	std::string::size_type pos = std::string("abtnvfr").find(c);
	if (pos != std::string::npos) return pos + 7;
	switch (c) {
		case '\"': case '\'': case '\\': case '?':
			return c;
		case '0':
			return oct(*filerp++);
		case 'x':
			return hex(*filerp++);
	}
	*--filerp = c;
	Error(ERR_ESC);
	return 0;
}

static char skip_normal() {
	char c;
	while ((c = _getc()) && c <= ' ')
		;
	return c;
}

static char skip_tolf() {
	char c;
	while ((c = _getc()) && c <= ' ' && c != '\n')
		;
	return c;
}

static char skip_prepro() {
	char c;
	do {
		c = _getc();
		if (c == '#') {
			getch = &skip_tolf;
			PreproDirective();
			getch = &skip_prepro;
		}
	} while (c == '#' || (!PreproActive() && c));
	return c;
}

void LoadSource(char *buf) {
	_getc = &getc_compile;
	getch = &skip_normal;
	start = filerp = buf;
	lineNum = lineNumLast = 1;
	fileStack.clear();
	fileStack.emplace_back(new Buf);
}

void FilePrepro(const char *sourcename, std::string &_dst) {
	dst = &_dst;
	_getc = &getc_prepro;
	getch = &skip_prepro;
	fileStack.clear();
	filerp = Open(sourcename);
	start = fileStack.back()->buf;
	lineNum = lineNumLast = 1;
	fileStack.back()->path = sourcename;
	printLine();
}
