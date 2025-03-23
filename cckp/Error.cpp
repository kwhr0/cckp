#include "Error.h"
#include "File.h"

static const char *errStr[] = {
	"internal error",
	// file
	"cannot open %s",
	"cannot write %s",
	"preprocessor error",
	"comment not closed %s",
	"needs value",
	"needs %c",
	"illegal escape",
	// expr
	"needs constant expression",
	"illegal type",
	"needs operator or undefined type",
	"multiple defined: %s",
	"unbalance () or unterminated ?:",
	"extra word: %s",
	// node
	"undefined: %s",
	"not a pointer",
	"illegal pointer arithmetic",
	"cannot get address",
	"not a function",
	"not a struct",
	"cannot struct",
	"%s is not a member",
	"expression too complex",
	"cannot substitution",
	"argument count",
	"argument type mismatch (#%d)",
	"cannot float",
	"cannot const",
	// context
	"illegal break",
	"illegal continue",
	"needs while",
	"unknown keyword: %s",
	"illegal variable size",
	"unknown type",
	"already defined type",
	// link
	"link: undefined: %s",
	"link: multiple defined: %s",
	// prepro
	"unterminated if",
	"include",
	"prepro directive",
	"macro argument",
	"",
};

static int errors, fatals;

void ErrorInit() {
	errors = fatals = 0;
}

void Error(int x, ...) {
	va_list args;
	va_start(args, x);
	int pos = 0;
	char *s = ErrorInfo(&pos);
	fprintf(stderr, "\033[1m\033[31merror: \033[0m");
	vfprintf(stderr, errStr[x], args);
	if (s) {
		putc('\n', stderr);
		for (int i = 0; s[i] && s[i] != '\n'; i++)
			if (s[i] == '\t') fprintf(stderr, "    ");
			else putc(s[i], stderr);
		putc('\n', stderr);
		while (pos-- >= 0)
			if (s[pos] == '\t') fprintf(stderr, "    ");
			else putc(' ', stderr);
		putc('^', stderr);
	}
	putc('\n', stderr);
	va_end(args);
	if (++errors >= 10) {
		fprintf(stderr, "Too many errors.\n");
		throw FatalException();
	}
	if (s) throw ErrorException();
}

void Fatal(int x, ...) {
	va_list args;
	va_start(args, x);
	ErrorInfo();
	fprintf(stderr, "\033[1m\033[31mfatal: \033[0m");
	vfprintf(stderr, errStr[x], args);
	fprintf(stderr, "\n");
	fatals++;
	va_end(args);
	throw FatalException();
}

int ErrorCount() {
	return fatals + errors;
}
