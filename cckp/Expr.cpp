#include "Expr.h"
#include "Error.h"
#include "File.h"
#include "Node.h"
#include "Prepro.h"

static Node *contents;
static int labelNum, condCount;
static bool isPrepro;

void ExprInit(bool prepro) {
	contents = nullptr;
	labelNum = condCount = 0;
	isPrepro = prepro;
}

static Type *DefineStructOrUnion(std::string *tname, Type *type, Type::Attr attr, bool isStruct, bool define) {
	if (define) _getc();
	if (!type) type = new Type(Type::STRUCT);
	type->attr = attr;
	if (tname) G::scopeStack.back().types[*tname] = type;
	if (!define) return type;
	for (int ofs = 0; Type *type_base = BaseType();) {
		char c;
		do {
			Type *mtype = type_base;
			std::string s = ParseType(mtype);
			if (std::find_if(begin(type->members), end(type->members), [&](auto m) { return m.first == s; }) != end(type->members))
				Error(ERR_MULTI, s.c_str());
			Align(ofs, mtype->size);
			type->members.emplace_back(s, Member(mtype, ofs));
			if (isStruct) ofs += mtype->size;
			type->size = std::max(type->size, isStruct ? ofs : mtype->size);
			c = getch();
		} while (c == ',');
		assertch(c, ';');
	}
	Align(type->size, UNIT_SIZE);
	needch('}');
	return type;
}

static Type *DefineEnum(std::string *tname) {
	_getc();
	Type *type = new Type(Type::VALUE, UNIT_SIZE);
	if (tname) G::scopeStack.back().types[*tname] = type;
	long v = 0;
	char c = 0;
	do {
		std::string s = getword();
		if (G::scopeStack.back().consts.count(s)) Error(ERR_MULTI, s.c_str());
		if ((c = getch()) == '=') v = ConstExpr(true);
		else _ungetc(c);
		G::scopeStack.back().consts[s] = v++;
		c = getch();
	} while (c == ',');
	assertch(c, '}');
	return type;
}

Type *BaseType() {
	Type::Attr attr;
	auto search = [&](std::string &s) {
		for (auto i = rbegin(G::scopeStack); i != rend(G::scopeStack); i++)
			if (i->types.count(s)) {
				Type *type = i->types[s];
				if (type->kind != Type::STRUCT || type->size) type = new Type(*type);
				type->attr = attr;
				type->attr._unsigned |= i->types[s]->attr._unsigned;
				type->attr._float |= i->types[s]->attr._float;
				return type;
			}
		return (Type *)nullptr;
	};
	auto tagtype = [&](std::string &s, auto func) {
		if (std::string tag = getword(); tag.length()) {
			std::string tname = s + " " + tag;
			Type *t = search(tname);
			if (t && t->size) return t;
			return func(&tname, t, chkch() == '{');
		}
		return chkch() == '{' ? func(nullptr, nullptr, true) : nullptr;
	};
	bool _short = false;
	int _long = 0;
	Type *t = nullptr;
	std::string s;
	while ((s = getword()).length()) {
		if (s == "struct" || s == "union")
			return tagtype(s, [&](std::string *tname, Type *type, bool define) {
				return DefineStructOrUnion(tname, type, attr, s == "struct", define);
			});
		else if (s == "enum") return tagtype(s, [&](std::string *tname, Type *, bool) { return DefineEnum(tname); });
		else if (s == "signed") attr._unsigned = false;
		else if (s == "unsigned") attr._unsigned = true;
		else if (s == "static") attr._static = true;
		else if (s == "inline" || s == "__inline__") attr._inline = true;
		else if (s == "extern") attr._extern = true;
		else if (s == "float" || s == "double") attr._float = true;
		else if (s == "short") _short = true;
		else if (s == "long") _long++; // (ISA64) long is 32-bit, long long is 64-bit
		else if (s == "const") attr._const = true;
		else if (s != "volatile" && s != "register" && s != "auto") {
			t = search(s);
			break;
		}
	}
	if (!t) {
		ungets(s);
		if (attr._float || attr._unsigned || _short || _long) {
			t = new Type(Type::VALUE, INTEGER_SIZE);
			t->attr = attr;
		}
	}
	if (t) {
		if (_short) t->size = 2;
		else if (_long > 1 || attr._float) t->size = UNIT_SIZE;
	}
	return t;
}

std::string ParseType(Type *&top) {
	struct TypeO : Type {
		TypeO(Kind _kind, int _size, int _prio) : Type(_kind, _size), prio(_prio) {}
		int prio;
	};
	std::vector<TypeO *> stack;
	bool dup = false;
	auto make1 = [&](auto cond) {
		for (TypeO *t; !stack.empty() && cond((t = stack.back())->prio);) {
			stack.pop_back();
			if (!dup) {
				dup = true;
				top = new Type(*top);
			}
			if (t->kind == Type::ARRAY) t->size *= top->size;
			t->next = top;
			t->attr._static = top->attr._static;
			t->attr._extern = top->attr._extern;
			t->attr._varg = top->attr._varg;
			t->attr._inline = top->attr._inline;
			top = t;
		}
	};
	std::string name;
	int prio = 0;
	bool vf = false, exitf = false;
	do {
		TypeO *type = nullptr;
		switch (char c = getch()) {
			case '(':
				if (!vf) prio += 0x10;
				else {
					type = new TypeO(Type::FUNC, 0, prio + 1);
					do {
						if (chkch() == '.') {
							_getc();
							if (_getc() != '.') Error(ERR_TYPE);
							if (_getc() != '.') Error(ERR_TYPE);
							c = getch();
							top->attr._varg = true;
							break;
						}
						if (Type *type1 = BaseType()) {
							std::string argname = ParseType(type1);
							if (type1->kind == Type::ARRAY) {
								type1->kind = Type::POINTER;
								type1->size = UNIT_SIZE;
							}
							if (type1->size) type->args.emplace_back(argname, type1);
						}
						c = getch();
					} while (c == ',');
					assertch(c, ')');
				}
				break;
			case ')':
				if (prio > 0) {
					vf = true;
					do prio -= 0x10;
					while ((c = getch()) == ')' && prio > 0);
					_ungetc(c);
				}
				else {
					_ungetc(c);
					exitf = true;
				}
				break;
			case '*':
				type = new TypeO(Type::POINTER, UNIT_SIZE, prio);
				break;
			case '[':
				type = new TypeO(Type::ARRAY, chkch() == ']' ? 0 : (int)ConstExpr(), prio + 1);
				needch(']');
				break;
			default:
				_ungetc(c);
				if (std::string s = getword(); s.length()) {
					if (s == "const") (stack.empty() ? top : stack.back())->attr._const = true;
					else {
						if (vf) Error(ERR_EXTRA_WORD, s.c_str());
						name = s;
						vf = true;
					}
				}
				else exitf = true;
				break;
		}
		if (type) {
			make1([type](int prio) { return type->prio > prio; });
			stack.push_back(type);
		}
	} while (!exitf);
	make1([](int) { return true; });
	return name;
}

static Node *getpre() {
	char c;
	while ((c = getch()) == '(') {
		if (Type *type = BaseType()) { // cast operator
			ParseType(type);
			needch(')');
			return new Node(type);
		}
		else Node::prio += 0x10;
	}
	switch (c) {
		case '+':
			if ((c = _getc()) == '+') return new Node(Node::PRE_PLUS);
			_ungetc(c);
			return new Node(Node::SPLUS);
		case '-':
			if ((c = _getc()) == '-') return new Node(Node::PRE_MINUS);
			_ungetc(c);
			return new Node(Node::SMINUS);
		case '*':
			return new Node(Node::POINTER);
		case '&':
			return new Node(Node::ADDRESS);
		case '!':
			return new Node(Node::NOT);
		case '~':
			return new Node(Node::COM);
	}
	_ungetc(c);
	return nullptr;
}

static Node *getop(bool commaInhibit) {
	char c = getch(), c1;
	switch (c) {
		case '=':
			if ((c = _getc()) == '=') return new Node(Node::EQ);
			_ungetc(c);
			return new Node(Node::SUBST);
		case '>':
			if ((c = _getc()) == '=') return new Node(Node::GE);
			else if (c == '>') {
				if ((c = _getc()) == '=') return new Node(Node::SR_SUBST);
				_ungetc(c);
				return new Node(Node::SR);
			}
			_ungetc(c);
			return new Node(Node::GT);
		case '<':
			if ((c = _getc()) == '=') return new Node(Node::LE);
			else if (c == '<') {
				if ((c = _getc()) == '=') return new Node(Node::SL_SUBST);
				_ungetc(c);
				return new Node(Node::SL);
			}
			_ungetc(c);
			return new Node(Node::LT);
		case '!':
			if ((c1 = _getc()) == '=') return new Node(Node::NE);
			_ungetc(c1);
			_ungetc(c);
			return nullptr;
		case '&':
			if ((c = _getc()) == '&') return new Node(Node::ANDAND);
			else if (c == '=') return new Node(Node::AND_SUBST);
			_ungetc(c);
			return new Node(Node::AND);
		case '|':
			if ((c = _getc()) == '|') return new Node(Node::OROR);
			else if (c == '=') return new Node(Node::OR_SUBST);
			_ungetc(c);
			return new Node(Node::OR);
		case '^':
			if ((c = _getc()) == '=') return new Node(Node::XOR_SUBST);
			_ungetc(c);
			return new Node(Node::XOR);
		case '+':
			if ((c = _getc()) == '+') return new Node(Node::POST_PLUS);
			else if (c == '=') return new Node(Node::PLUS_SUBST);
			_ungetc(c);
			return new Node(Node::PLUS);
		case '-':
			if ((c = _getc()) == '-') return new Node(Node::POST_MINUS);
			else if (c == '=') return new Node(Node::MINUS_SUBST);
			else if (c == '>') return new Node(Node::ARROW);
			_ungetc(c);
			return new Node(Node::MINUS);
		case '*':
			if ((c = _getc()) == '=') return new Node(Node::MUL_SUBST);
			_ungetc(c);
			return new Node(Node::MUL);
		case '/':
			if ((c = _getc()) == '=') return new Node(Node::DIV_SUBST);
			_ungetc(c);
			return new Node(Node::DIV);
		case '%':
			if ((c = _getc()) == '=') return new Node(Node::REM_SUBST);
			_ungetc(c);
			return new Node(Node::REM);
		case '(':
			return new Node(Node::CALL);
		case '[':
			return new Node(Node::ARRAY);
		case '.':
			return new Node(Node::DOT);
		case '?':
			condCount++;
			return new Node(Node::COND);
		case ':':
			if (!condCount) break;
			condCount--;
			return new Node(Node::COLON);
		case ',':
			if (!Node::prio && commaInhibit) break;
			return new Node(Node::COMMA);
	}
	_ungetc(c);
	return nullptr;
}

std::string StringLiteral(int *np) {
	needch('\"');
	char label[16];
	sprintf(label, "string%d", ++labelNum);
	Obj::code.emplace_back(Obj::DATA, label, G::dataOfs);
	int c, i, len = 1;
	do {
		std::string s = _gets();
		for (i = 0; i < s.length(); i++) Obj::code.emplace_back(Obj::DEF, "", 1, s[i]);
		len += i;
		needch('\"');
	} while ((c = getch()) == '\"');
	_ungetc(c);
	Obj::code.emplace_back(Obj::DEF, "", 1);
	Obj::code.emplace_back(Obj::ENDDATA);
	if (np) *np = len;
	G::dataOfs += len;
	return label;
}

static Node *Make(bool commaInhibit) {
	int lastprio = Node::prio;
	Node::prio = 0;
	std::vector<Node *> tStack, opStack;
	auto make1 = [&](auto cond) {
		for (Node *op; !opStack.empty() && cond((op = opStack.back())->op_prio);) {
			opStack.pop_back();
			if (op->kind == Node::COND) {
				op->c2 = tStack.back();
				tStack.pop_back();
			}
			if (!op->isUnary()) {
				op->c1 = tStack.back();
				tStack.pop_back();
			}
			op->c0 = tStack.back();
			tStack.back() = op;
		}
	};
	bool vf = false;
	auto put = [&](Node *node) {
		tStack.push_back(node);
		if (vf) Error(ERR_NEEDS_OP);
		vf = true;
	};
	do if (Node *node = vf ? getop(commaInhibit) : getpre()) {
		make1([&](int prio) { return node->isRight() ? node->op_prio < prio : node->op_prio <= prio; });
		if (node->kind != Node::COLON) opStack.push_back(node);
		if (node->kind == Node::ARRAY) {
			vf = false;
			put(Make(false));
			needch(']');
		}
		else if (node->kind == Node::CALL) {
			while (Node *a = Make(true)) {
				node->func_arg.push_back(a);
				if (chkch() == ',') _getc();
			}
			needch(')');
		}
		else vf &= node->kind == Node::POST_PLUS || node->kind == Node::POST_MINUS;
	}
	else if (std::string s = getword(); s == "sizeof") {
		bool f = chkch() == '(';
		if (f) _getc();
		if (Type *type = BaseType()) {
			ParseType(type);
			put(new Node(type->size));
		}
		else {
			Node *node1 = Make(commaInhibit);
			node1->Process();
			put(new Node(node1->type ? node1->type->size : 0));
		}
		if (f) needch(')');
	}
	else if (s == "defined") {
		bool f = chkch() == '(';
		if (f) _getc();
		put(new Node(MacroIsDefined()));
		if (f) needch(')');
	}
	else if (s.length()) put(isPrepro ? new Node(0) : new Node(s.c_str()));
	else {
		ungets(s);
		if (char c = chkch(); isnumber(c)) {
			long vi = 0;
			FLOAT v = 0;
			put(new Node(getvalue(vi, v), vi, v));
		}
		else if (c == '\'') {
			_getc();
			c = getcsub();
			put(new Node(c == '\\' ? escape() : c));
			assertch(_getc(), '\'');
		}
		else if (c == '\"') {
			Node *node1 = new Node(StringLiteral(nullptr));
			node1->type = new Type(Type::POINTER, UNIT_SIZE);
			node1->type->next = new Type(Type::VALUE, 1);
			put(node1);
		}
		else if (c == ')' && Node::prio > 0) {
			_getc();
			vf = true;
			do Node::prio -= 0x10;
			while ((c = getch()) == ')' && Node::prio > 0);
			_ungetc(c);
		}
		else break;
	} while (1);
	make1([](int){ return true; });
	Node::prio = lastprio;
	return tStack.empty() ? nullptr : tStack.back();
}

char ExprContext(bool dump, bool commaInhibit, Type *type) {
	char c;
	try {
		contents = Make(commaInhibit);
		if (condCount || Node::prio) Error(ERR_UNBALANCE);
		if (contents) {
			contents->Process();
			if (type) Node::ImplicitCast(type, contents);
			if (dump) contents->EmitAll();
		}
		c = getch();
	}
	catch (ErrorException) {
		while ((c = _getc()) && c != ';')
			;
		contents = nullptr;
	}
	return c;
}

long ConstExpr(bool commaInhibit) { // integer only
	Node *t = contents = Make(commaInhibit);
	if (condCount || Node::prio) Error(ERR_UNBALANCE);
	if (!t) Error(ERR_CONST_EXPR);
	t->Process();
	if (t->kind == Node::CAST) t = t->c0;
	if (t->kind != Node::CONST) Error(ERR_CONST_EXPR);
	return t->const_value;
}

Node *ExprContents() {
	return contents;
}
