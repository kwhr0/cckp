#include "Context.h"
#include "Error.h"
#include "Expr.h"
#include "File.h"
#include "Node.h"

static int labelNum, returnLabelNum, stackMin;
static std::vector<int> _break, _continue;
static std::set<std::string> gotoDefined, gotoNeed;
static Type *retType;

static void ParseStatement();

static void ParseBlock() {
	if (char c = chkch(); c == '{') {
		_getc();
		G::scopeStack.emplace_back(G::scopeStack.back().ofs);
		for (c = chkch(); c && c != '}'; c = chkch()) ParseBlock();
		G::scopeStack.pop_back();
		needch('}');
	}
	else ParseStatement();
}

static int ArrayInit(Type *type) {
	int n = 0;
	if (chkch() == '\"') {
		StringLiteral(&n);
		if (!type->size) type->size = n;
	}
	else {
		needch('{');
		char c = 0;
		do {
			c = ExprContext(false, true, type);
			Node *contents = ExprContents();
			if (!contents) break;
			if (!type->size || n < type->size) contents->EmitLiteral(Obj::DEF, type->next, type->next->size);
			n += type->next->size;
		} while (c == ',');
		if (type->size)
			for (; n < type->size; n += type->next->size)
				Obj::code.emplace_back(Obj::DEF, "", type->next->size);
		else type->size = n;
		if (c) _ungetc(c);
		needch('}');
	}
	return n;
}

static char CompileTimeInit(Type *type) {
	int pad, n = 0, l;
	char c = 0;
	Node *contents;
	switch (type->kind) {
		case Type::VALUE:
			c = ExprContext(false, true, type);
			contents = ExprContents();
			if (!contents) break;
			contents->EmitLiteral(Obj::DEF, type, type->size);
			if (c) _ungetc(c);
			break;
		case Type::POINTER:
			if ((c = chkch()) == '\"' || c == '{') {
				l = ++labelNum;
				Obj::code.emplace_back(Obj::DATA, "pointer", G::dataOfs, l);
				Align(G::dataOfs += ArrayInit(type));
				Obj::code.emplace_back(Obj::DEF, "pointer", UNIT_SIZE, l);
			}
			else {
				Obj::code.emplace_back(Obj::DATA, "", G::dataOfs);
				Obj::code.emplace_back(Obj::DEF, "", type->size, ConstExpr());
			}
			Obj::code.emplace_back(Obj::ENDDATA);
			break;
		case Type::ARRAY:
			ArrayInit(type);
			break;
		case Type::STRUCT:
			needch('{');
			for (auto m : type->members) {
				n += pad = m.second.ofs - n;
				while (--pad >= 0) Obj::code.emplace_back(Obj::DEF, "", 1);
				c = CompileTimeInit(m.second.type);
				n += m.second.type->size;
				if (c != ',') break;
			}
			if (c) _ungetc(c);
			needch('}');
			break;
		case Type::FUNC:
			break;
	}
	return getch();
}

static char RuntimeInit(Type *type, int ofs, int *arrayNum = nullptr) {
	Node *contents;
	int i = 0;
	char c = 0;
	switch (type->kind) {
		case Type::VALUE: case Type::POINTER:
			c = ExprContext(false, true, type);
			contents = ExprContents();
			if (!contents) break;
			contents->EmitAll(1);
			Obj::code.emplace_back(Obj::ST, REGNUM_BASE + 1, ofs, REGNUM_BASE, type->size);
			break;
		case Type::ARRAY:
			if ((c = chkch()) == '\"') {
				Obj::code.emplace_back(Obj::LIL, StringLiteral(&i), REGNUM_BASE + 1);
				Obj::code.emplace_back(Obj::LABEL, "copy",  ++labelNum);
				Obj::code.emplace_back(Obj::LD, REGNUM_BASE + 2, 0, REGNUM_BASE + 1, 1);
				Obj::code.emplace_back(Obj::ADDI, REGNUM_BASE + 1, 1);
				Obj::code.emplace_back(Obj::ST, REGNUM_BASE + 2, 0, REGNUM_BASE, 1);
				Obj::code.emplace_back(Obj::ADDI, REGNUM_BASE, 1);
				Obj::code.emplace_back(Obj::BNE, "copy", REGNUM_BASE + 2, REGNUM_ZERO, labelNum);
				if (arrayNum) *arrayNum = i;
				c = getch();
				break;
			}
			needch('{');
			for (i = 1; (c = RuntimeInit(type->next, ofs)) == ','; i++) ofs += type->next->size;
			if (arrayNum) *arrayNum = i;
			if (c) _ungetc(c);
			needch('}');
			c = getch();
			break;
		case Type::STRUCT:
			needch('{');
			for (auto m : type->members)
				if ((c = RuntimeInit(m.second.type, m.second.ofs)) != ',') break;
			if (c) _ungetc(c);
			needch('}');
			c = getch();
			break;
		case Type::FUNC:
			break;
	}
	return c;
}

static void DefineLocalVar(Type *type_base) {
	auto preofs = [](Scope &sc, int size) {
		int align = std::min(UNIT_SIZE, size);
		int n = (sc.ofs -= size) % align; // n is 0 or minus
		if (n) sc.ofs -= align + n;
		if (stackMin > sc.ofs) stackMin = sc.ofs;
	};
	char c;
	do {
		Type *type = type_base;
		std::string name = ParseType(type);
		Scope &sc = type->attr._static || type->attr._extern ? G::scopeStack.front() : G::scopeStack.back();
		if (sc.vars.count(name)) Error(ERR_MULTI, name.c_str());
		if ((c = getch()) == '=')
			if (type->attr._static) {
				Align(G::dataOfs);
				sc.vars[name] = Var(type, G::dataOfs, true, true);
				Obj::code.emplace_back(Obj::DATA, "", G::dataOfs);
				c = CompileTimeInit(type);
				Obj::code.emplace_back(Obj::ENDDATA);
				G::dataOfs += type->size;
			}
			else {
				size_t save = std::distance(begin(Obj::code), end(Obj::code));
				Obj::code.emplace_back(Obj::LEA, REGNUM_BASE, sc.ofs, REGNUM_SP, 0, name);
				int arrayNum;
				c = RuntimeInit(type, 0, &arrayNum);
				if (type->kind == Type::ARRAY && !type->size) type->size = type->next->size * arrayNum;
				preofs(sc, type->size);
				Obj::code[save].imm = sc.ofs;
				sc.vars[name] = Var(type, sc.ofs, false, false);
				if (type->kind == Type::VALUE && type->attr._const) {
					Node *contents = ExprContents();
					if (contents->kind == Node::CONST) {
						sc.consts[name] = contents->const_value;
						sc.consts_f[name] = contents->const_value_f;
					}
				}
			}
		else {
			if (type->attr._static) {
				Align(sc.ofs);
				Obj::code.emplace_back(Obj::SPACE, name, sc.ofs, type->size);
			}
			else if (!type->attr._extern) preofs(sc, type->size);
			sc.vars[name] = Var(type, sc.ofs, type->attr._static || type->attr._extern, false);
			if (type->attr._static) sc.ofs += type->size;
		}
	} while (c == ',');
	assertch(c, ';');
}

static void DefineGlobalVar(std::string &name, Type *type, bool dataf) {
	Scope &sc = G::scopeStack.front();
	// redefinition of global variable is not an error.
	//if (sc.vars.count(name) && sc.vars[name].defined) Error(ERR_MULTI, name.c_str());
	if (type->attr._extern)
		sc.vars[name] = Var(type, 0, true, false); // offset will be set by linker
	else {
		if (!type->size) Error(ERR_VAR_SIZE);
		int &ofs = dataf ? G::dataOfs : sc.ofs;
		Align(ofs);
		sc.vars[name] = Var(type, ofs, true, dataf, true);
		if (!dataf) Obj::code.emplace_back(Obj::SPACE, name, ofs, type->size);
		ofs += type->size;
	}
}

static void DefineFunction(std::string &name, Type *type) {
	int ofs = UNIT_SIZE; // next of return address
	if (type->next->kind == Type::STRUCT) ofs += type->next->size;
	G::scopeStack.emplace_back();
	for (auto &p : type->args) {
		G::scopeStack.back().vars[p.first] = Var(p.second, ofs, false, false);
		ofs += std::max(type->size, UNIT_SIZE);
	}
	returnLabelNum = ++labelNum;
	Obj::code.emplace_back(Obj::LABEL, name);
	Obj::code.emplace_back(Obj::COMMENT, "FUNC " + name);
	int save = (int)std::distance(begin(Obj::code), end(Obj::code));
	stackMin = 0;
	gotoNeed.clear();
	gotoDefined.clear();
	Node::stackArg.clear();
	ParseBlock();
	for (const std::string &s : gotoNeed)
		if (!gotoDefined.count(s)) Error(ERR_UNDEF, s.c_str());
	int usedregs = Node::GetUsedRegs(), r = REGNUM_ARG;
	std::vector<Obj> so;
	ofs = UNIT_SIZE;
	for (auto &p : type->args) {
		if (p.second->kind != Type::STRUCT) {
			if (r > REGNUM_ARGEND) break;
			if (Node::stackArg.count(ofs)) so.emplace_back(Obj::ST, r, ofs, REGNUM_SP, UNIT_SIZE);
			else Obj::ReplaceArg(save, ofs, r);
			usedregs &= ~(1 << r++);
		}
		ofs += std::max(p.second->size, UNIT_SIZE);
	}
	int n = stackMin % UNIT_SIZE; // n is 0 or minus
	if (n) stackMin -= UNIT_SIZE + n;
	n = 0;
	for (uint32_t i = usedregs; i; i >>= 1) n += i & 1;
	int frame = UNIT_SIZE * n - stackMin;
	Obj::AdjustOffset(save, [&](bool isArg) { return isArg ? frame : -stackMin; });
	Obj::code.emplace_back(Obj::LABEL, "return", returnLabelNum);
	if (frame) so.emplace_back(Obj::ADDI, REGNUM_SP, -frame);
	ofs = -stackMin;
	for (int i = REGNUM_BASE; i <= REGNUM_ARGEND; i++)
		if (usedregs & 1 << i) {
			so.emplace_back(Obj::ST, i, ofs, REGNUM_SP, UNIT_SIZE, "save");
			Obj::code.emplace_back(Obj::LD, i, ofs, REGNUM_SP, UNIT_SIZE, "restore");
			ofs += UNIT_SIZE;
		}
	Obj::code.insert(begin(Obj::code) + save, begin(so), end(so));
	if (frame) Obj::code.emplace_back(Obj::ADDI, REGNUM_SP, frame);
	G::scopeStack.pop_back();
	if (type->attr._inline) Obj::MakeInline(name, save);
	else Obj::code.emplace_back(GetInterruptNum(name.c_str()) ? Obj::RETI : Obj::RET);
}

static void parse_if() {
	needch('(');
	assertch(ExprContext(true), ')');
	int l = ++labelNum;
	Obj::code.emplace_back(Obj::BEQ, "else", REGNUM_BASE, REGNUM_ZERO, l);
	ParseBlock();
	if (std::string s = getword(); s == "else") {
		Obj::code.emplace_back(Obj::BRA, "endif", l);
		Obj::code.emplace_back(Obj::LABEL, "else", l);
		ParseBlock();
	}
	else {
		ungets(s);
		Obj::code.emplace_back(Obj::LABEL, "else", l);
	}
	Obj::code.emplace_back(Obj::LABEL, "endif", l);
}

static void parse_while() {
	int l = ++labelNum;
	_break.push_back(l);
	_continue.push_back(l);
	needch('(');
	Obj::code.emplace_back(Obj::BRA, "while_tail", l);
	Obj::code.emplace_back(Obj::LABEL, "while_top", l);
	assertch(ExprContext(false), ')');
	Node *cond = ExprContents();
	ParseBlock();
	Obj::code.emplace_back(Obj::LABEL, "while_tail", l);
	Obj::code.emplace_back(Obj::LABEL, "continue", l);
	cond->EmitAll();
	Obj::code.emplace_back(Obj::BNE, "while_top", REGNUM_BASE, REGNUM_ZERO, l);
	Obj::code.emplace_back(Obj::LABEL, "break", l);
	_continue.pop_back();
	_break.pop_back();
}

static void parse_for() {
	int l = ++labelNum;
	_break.push_back(l);
	_continue.push_back(l);
	G::scopeStack.emplace_back(G::scopeStack.back().ofs);
	needch('(');
	if (chkch() == ';') _getc();
	else if (Type *type = BaseType()) DefineLocalVar(type);
	else assertch(ExprContext(true), ';');
	Obj::code.emplace_back(Obj::BRA, "for_tail", l);
	Obj::code.emplace_back(Obj::LABEL, "for_top", l);
	Node *cond = nullptr, *next = nullptr;
	if (chkch() != ';') {
		assertch(ExprContext(false), ';');
		cond = ExprContents();
	}
	else _getc();
	if (chkch() != ')') {
		assertch(ExprContext(false), ')');
		next = ExprContents();
	}
	else _getc();
	ParseBlock();
	Obj::code.emplace_back(Obj::LABEL, "continue", l);
	if (next) next->EmitAll();
	Obj::code.emplace_back(Obj::LABEL, "for_tail", l);
	if (cond) {
		cond->EmitAll();
		Obj::code.emplace_back(Obj::BNE, "for_top", REGNUM_BASE, REGNUM_ZERO, l);
	}
	else Obj::code.emplace_back(Obj::BRA, "for_top", l);
	Obj::code.emplace_back(Obj::LABEL, "break", l);
	G::scopeStack.pop_back();
	_continue.pop_back();
	_break.pop_back();
}

static void parse_do() {
	int l = ++labelNum;
	_break.push_back(l);
	_continue.push_back(l);
	Obj::code.emplace_back(Obj::LABEL, "do", l);
	ParseBlock();
	Obj::code.emplace_back(Obj::LABEL, "continue", l);
	if (getword() != "while") Error(ERR_NEEDS_WHILE);
	needch('(');
	assertch(ExprContext(true), ')');
	needch(';');
	Obj::code.emplace_back(Obj::BNE, "do", REGNUM_BASE, REGNUM_ZERO, l);
	Obj::code.emplace_back(Obj::LABEL, "break", l);
	_continue.pop_back();
	_break.pop_back();
}

static void parse_switch() {
	int l = ++labelNum, i = 0;
	_break.push_back(l);
	needch('(');
	assertch(ExprContext(true), ')');
	needch('{');
	size_t save = std::distance(begin(Obj::code), end(Obj::code));
	std::vector<long> caseValue;
	bool defaultValid = false;
	while (chkch() != '}')
		if (std::string s = getword(); s == "case") {
			Obj::code.emplace_back(Obj::LABEL, "case", l, ++i);
			caseValue.push_back(ConstExpr());
			needch(':');
		}
		else if (s == "default") {
			Obj::code.emplace_back(Obj::LABEL, "default", l);
			defaultValid = true;
			needch(':');
		}
		else {
			ungets(s);
			ParseBlock();
		}
	needch('}');
	std::vector<Obj> t;
	i = 0;
	for (long v : caseValue) {
		t.emplace_back(Obj::LI, REGNUM_BASE + 1, v);
		t.emplace_back(Obj::BEQ, "case", REGNUM_BASE + 1, REGNUM_BASE, l, ++i);
	}
	t.emplace_back(Obj::BRA, defaultValid ? "default" : "break", l);
	Obj::code.insert(begin(Obj::code) + save, begin(t), end(t));
	Obj::code.emplace_back(Obj::LABEL, "break", l);
	_break.pop_back();
}

static void parse_break() {
	needch(';');
	if (!_break.empty()) Obj::code.emplace_back(Obj::BRA, "break", _break.back());
	else Error(ERR_BREAK);
}

static void parse_continue() {
	needch(';');
	if (!_continue.empty()) Obj::code.emplace_back(Obj::BRA, "continue", _continue.back());
	else Error(ERR_CONTINUE);
}

static void parse_return() {
	assertch(ExprContext(true, false, retType), ';');
	if (Node *c = ExprContents()) {
		if (c->type->kind == Type::STRUCT) {
			Obj::code.emplace_back(Obj::MOV, REGNUM_BASE + 1, REGNUM_BASE);
			Obj::code.emplace_back(Obj::LEA, REGNUM_BASE, UNIT_SIZE, REGNUM_SP, 0, "struct", Obj::ARG);
			Obj::_memcpy(REGNUM_BASE + 2, c->type->size);
			Obj::code.emplace_back(Obj::LEA, REGNUM_RET, UNIT_SIZE, REGNUM_SP, 0, "struct", Obj::ARG);
		}
		else Obj::code.emplace_back(Obj::MOV, REGNUM_RET, REGNUM_BASE);
	}
	Obj::code.emplace_back(Obj::BRA, "return", returnLabelNum);
}

static void parse_goto() {
	std::string s = getword();
	Obj::code.emplace_back(Obj::BRA, s);
	needch(';');
	gotoNeed.insert(s);
}

static void parse_typedef() {
	Type *type = BaseType();
	if (!type) Error(ERR_TYPEDEF_UNDEF);
	std::string name = ParseType(type);
	if (!name.length() || G::scopeStack.back().types.count(name)) Error(ERR_TYPEDEF_MULTI);
	G::scopeStack.back().types[name] = type;
	needch(';');
}

static std::map<std::string, void (*)()> keywords = {
#define S(x) { #x, parse_##x }
	S(if), S(while), S(for), S(do), S(switch), S(break), S(continue), S(return), S(goto), S(typedef),
};

static void ParseStatement() {
	if (Type *type = BaseType()) DefineLocalVar(type);
	else {
		if (std::string s = getword(); keywords.count(s)) keywords[s]();
		else if (char c = _getc(); c == ':') {
			Obj::code.emplace_back(Obj::LABEL, s);
			gotoDefined.insert(s);
		}
		else {
			_ungetc(c);
			ungets(s);
			assertch(ExprContext(true), ';');
		}
	}
}

static void init() {
	labelNum = returnLabelNum = stackMin = 0;
	_break.clear();
	_continue.clear();
	gotoDefined.clear();
	gotoNeed.clear();
	G::scopeStack.emplace_back();
	std::map<std::string, Type *> &t = G::scopeStack.front().types;
	t["void"] = new Type(Type::VALUE);
	t["char"] = new Type(Type::VALUE, 1);
	t["int"] = new Type(Type::VALUE, INTEGER_SIZE);
	(t["_Bool"] = new Type(Type::VALUE, 1))->attr._unsigned = true;
}

void ParseAll() {
	init();
	while (chkch()) {
		if (Type *type_base = BaseType())
			while (1) {
				bool dataf = false;
				Type *type = type_base;
				std::string name = ParseType(type);
				if (!name.length()) {
					needch(';');
					break;
				}
				char c = getch();
				if (c == '=') {
					Obj::code.emplace_back(Obj::DATA, "", G::dataOfs);
					c = CompileTimeInit(type);
					Obj::code.emplace_back(Obj::ENDDATA);
					dataf = true;
				}
				if (c == ';' && type->kind == Type::FUNC) {
					type->attr._extern = true;
					G::funcs[name] = type;
					break;
				}
				else if (c == ',' || c == ';') {
					DefineGlobalVar(name, type, dataf);
					if (c == ';') break;
				}
				else if (c == '{') {
					_ungetc(c);
					if (G::funcs.count(name)) {
						if (!G::funcs[name]->attr._extern) Error(ERR_MULTI, name.c_str());
						else if (G::funcs[name]->args.size() != type->args.size()) Error(ERR_ARG);
					}
					G::funcs[name] = type;
					retType = type->next;
					DefineFunction(name, type);
					break;
				}
				else break;
			}
		else if (std::string s = getword(); s == "typedef") parse_typedef();
		else Error(ERR_KEYWORD, s.c_str());
	}
}
