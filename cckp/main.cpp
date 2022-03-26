#include "Context.h"
#include "Expr.h"
#include "Error.h"
#include "File.h"
#include "Node.h"
#include "Link.h"
#include "Prepro.h"
#include <unistd.h>

static void compile(const char *path, bool depend, bool preproOnly) {
	ErrorInit();
	ExprInit(true);
	Node::StaticInit();
	Obj::StaticInit();
	G::funcs.clear();
	G::scopeStack.clear();
	G::dataOfs = 0;
	preprocess(path, depend, preproOnly);
	ExprInit(false);
	if (!depend) ParseAll();
}

int main(int argc, char *argv[]) {
#if DEBUG
	printf("---- DEBUG BUILD (%d-bit) ----\n", 8 * UNIT_SIZE);
#endif
	std::string archive;
	bool preproOnly = false, compileOnly = false, depend = false;
	try {
		std::string pwd = std::string(getenv("PWD")) + '/';
		includePath.push_back(pwd);
		for (int c; (c = getopt(argc, argv, "D:EI:Ma:cl:")) != -1;) {
			bool e = false;
			std::string name, content;
			switch (c) {
				case 'D':
					for (char *p = optarg; *p; p++)
						if (*p == '=') e = true;
						else (e ? content : name) += *p;
					macrosGlobal[name] = '\0' + content;
					break;
				case 'E':
					preproOnly = true;
					break;
				case 'I':
					includePath.push_back(std::string(*optarg == '/' ? "" : pwd) + optarg + '/');
					break;
				case 'M':
					depend = true;
					break;
				case 'a':
					archive = optarg;
					break;
				case 'c':
					compileOnly = true;
					break;
				case 'l':
					ReadLibPath(optarg);
					break;
			}
		}
		if (argc <= optind) {
			fprintf(stderr, "Usage: cckp <source files>...\n");
			return 0;
		}
		for (int i = optind; i < argc; i++) {
			std::string s = argv[i];
			if (std::string::size_type pos = s.rfind(".c"); pos != std::string::npos) {
				s.replace(pos, 2, ".o");
				unlink(s.c_str());
				compile(argv[i], depend, preproOnly);
				if (ErrorCount()) return ErrorCount();
				if (!depend && !preproOnly) {
					FILE *fi = fopen(s.c_str(), "w");
					if (!fi) Fatal(FATAL_WRITE, s.c_str());
					Obj::DumpAll(fi);
					fclose(fi);
				}
			}
		}
	}
	catch (...) {}
	FileClear();
	if (!preproOnly && !compileOnly) {
		if (depend) printDepend(A_OUT, true);
		else Link(argc, argv, archive);
	}
	return ErrorCount();
}
