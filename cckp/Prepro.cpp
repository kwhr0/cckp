#include "Prepro.h"
#include "Error.h"
#include "File.h"
#include "Expr.h"
#include <sys/stat.h>
#include <set>

#define CONCAT		'\xfd'
#define STR			'\xfe'
#define ARG			'\xff'

std::map<std::string, std::string> macrosGlobal;
std::vector<std::string> includePath;

static int ibe, ibf, ibl;
static bool expandInh;
static std::map<std::string, std::string> macros;
static std::set<std::string> dependPath, dependPathAll;

static void PreproInit() {
	ibe = ibf = ibl = 0;
	expandInh = false;
	macros = macrosGlobal;
	dependPath.clear();
}

bool PreproActive() { return !ibf; }

bool MacroIsDefined() {
	expandInh = true;
	return macros.count(getword()) != 0;
}

static void incsub(bool next) {
	std::string pathname;
	bool sys = false;
	char c = getch();
	if (c == '<') sys = true;
	else if (c != '\"') Error(ERR_INCLUDE);
	while ((c = _getc()) != (sys ? '>' : '\"')) pathname += c;
	if (ibf) return;
	if (pathname[0] == '/') {
		PushInc(pathname);
		dependPath.insert(pathname);
		return;
	}
	auto testAndInclude = [&](std::string path) {
		struct stat buf;
		if (stat(path.c_str(), &buf)) return false;
		if (next) return next = false;
		PushInc(path);
		dependPath.insert(path);
		return true;
	};
	std::string cur = getCurPath();
	std::string::size_type pos = cur.rfind("/");
	cur = pos != std::string::npos ? cur.substr(0, pos + 1) : "";
	if (testAndInclude(cur + pathname)) return;
	for (std::string &s : includePath) if (testAndInclude(s + pathname)) return;
	Fatal(FATAL_OPEN, pathname.c_str());
}

static void p_include() { incsub(false); }
static void p_include_next() { incsub(true); }

static void p_define() {
	expandInh = true;
	std::map<std::string, char> arg;
	std::string t, p, name = getword();
	char c = _getc();
	if (c == '(') {
		while (1)
			if ((p = getword()).length()) {
				arg[p] = char(arg.size() + 1);
				if ((c = getch()) == ')') break;
				assertch(c, ',');
			}
			else {
				needch(')');
				break;
			}
		t.push_back(arg.size());
	}
	else {
		_ungetc(c);
		t.push_back(-1);
	}
	chkch(); // skip spaces
	char mark = ARG;
	while ((c = _getc()) && c != '\n')
		if (c == '#') {
			if ((c = _getc()) == '#') {
				mark = CONCAT;
				while (!t.empty() && (t.back() == '\t' || t.back() == ' ')) t.pop_back();
				chkch(); // skip spaces
			}
			else mark = STR;
		}
		else if (isalpha(c) || c == '_') {
			_ungetc(c);
			if (arg.count(p = getword())) {
				t += mark;
				t += arg[p];
			}
			else t += p;
		}
		else t += c;
	if (!ibf) macros[name] = t;
}

static void p_undef() {
	expandInh = true;
	std::string p = getword();
	if (!ibf) macros.erase(p);
}

template<typename F> static void ifsub(F callback) {
	if (ibf & (1 << ++ibl) - 1)
		for (char c; (c = _getc()) && c != '\n';)
			;
	else (callback() ? ibe : ibf) |= 1 << ibl;
}

static void p_if() {
	ifsub([] { return ConstExpr(); });
}

static void p_ifdef() {
	ifsub([] { return MacroIsDefined(); });
}

static void p_ifndef() {
	ifsub([] { return !MacroIsDefined(); });
}

static void p_else() {
	if (ibe & 1 << ibl) ibf |= 1 << ibl;
	else ibf &= ~(1 << ibl);
}

static void p_elif() {
	p_else();
	ibl--;
	p_if();
}

static void p_endif() {
	ibe &= ~(1 << ibl);
	ibf &= ~(1 << ibl--);
}

static void p_error() {
	char c;
	while ((c = _getc()) && c != '\n')
		;
	_ungetc(c);
	if (!ibf) Error(ERR_NONE);
}

struct QChk {
	QChk() : esc(false), q(false) {}
	char update(char c) {
		if (esc) esc = false;
		else if (q && c == '\\') esc = true;
		else if (c == '\'' || c == '\"') q = !q;
		return c;
	}
	bool esc, q;
};

bool ExpandMacro(std::string &s, bool replace) {
	if (expandInh || !macros.count(s)) return false;
	char *mp = macros[s].data();
	std::vector<std::string> arg;
	char c = _getc(), argn = *mp++;
	if (argn != -1)
		if (c == '(')
			while ((c = _getc()) != ')') {
				while (c <= ' ') c = _getc();
				arg.emplace_back();
				QChk qchk;
				for (int l = 1; c && (qchk.q || (c != ',' && (l += (c == '(') - (c == ')')))); c = _getc())
					arg.back() += qchk.update(c);
				if (c != ',') break;
			}
		else _ungetc(c);
	else _ungetc(c);
	std::string buf;
	if (argn == -1 || argn == arg.size())
		while (*mp)
			if ((c = *mp++) == ARG || c == STR || c == CONCAT) {
				if (c == STR) buf.push_back('\"');
				std::string t = arg[*mp++ - 1];
				if (c == ARG) ExpandMacro(t, true);
				buf += t;
				if (c == STR) buf.push_back('\"');
			}
			else buf.push_back(mp[-1]);
	else Error(ERR_MACRO_ARG);
	if (replace) s = buf;
	else ungets(buf);
	return true;
}

static std::map<std::string, void (*)()> keywords = {
#define S(x) { #x, p_##x }
	S(include), S(include_next), S(define), S(undef), S(if), S(ifdef), S(ifndef), S(else), S(elif), S(endif), S(error),
};

void PreproDirective() {
	std::string p = getword();
	if (keywords.count(p)) keywords[p]();
	else if (!ibf) Error(ERR_PREPRO);
	expandInh = false;
}

void printDepend(const std::string &s, bool all) {
	printf("%s: ", s.c_str());
	for (const std::string &s1 : all ? dependPathAll : dependPath) printf("%s ", s1.c_str());
	putchar('\n');
}

void preprocess(const char *path, bool depend, bool output) {
	static std::string src;
	PreproInit();
	FilePrepro(path, src);
	QChk qchk;
	src.clear();
	for (char c; (c = qchk.q ? _getc() : getch());) {
		AdjustLineNum(c);
		if (!qchk.q && (isalpha(c) || c == '_')) {
			_ungetc(c);
			src += getword();
		}
		else src += qchk.update(c);
	}
	if (ibl) Error(ERR_UNTERMINATED_IF);
	macros.clear();
	if (depend) {
		std::string s = path;
		if (std::string::size_type pos = s.rfind(".c"); pos != std::string::npos) {
			s.replace(pos, 2, ".o");
			printDepend(s, false);
		}
		dependPathAll.merge(dependPath);
	}
	else if (output)
		for (char *p = src.data(); *p;) putchar(*p++);
	else LoadSource(src.data());
}
