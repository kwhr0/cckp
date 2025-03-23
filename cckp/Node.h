#include "Obj.h"

struct Node {
	enum Kind {
		NOP, CONST, VAR, NAME, COND1, MEMBER, COMMA,
		RIGHT_START1 = 0x10, SUBST_START,
		SUBST, PLUS_SUBST, MINUS_SUBST, MUL_SUBST, DIV_SUBST, REM_SUBST, SL_SUBST, SR_SUBST, AND_SUBST, OR_SUBST, XOR_SUBST,
		SUBST_END,
		COND = 0x20, COLON, RIGHT_END1,
		OROR = 0x30, ANDAND = 0x40,
		OR = 0x50, XOR = 0x60, AND = 0x70,
		EQ = 0x80, NE, GT = 0x90, GE, LT, LE,
		SL = 0xa0, SR,
		PLUS = 0xb0, MINUS, MUL = 0xc0, DIV, REM,
		UNARY_START = 0xe0, RIGHT_START2,
		PRE_PLUS, PRE_MINUS, SPLUS, SMINUS, POINTER, ADDRESS, CAST, NOT, COM, RIGHT_END2,
		POST_PLUS = 0xf0, POST_MINUS, CALL, UNARY_END,
		ARRAY, DOT, ARROW
	};
	Node(Kind _kind) : kind(_kind), type(nullptr), op_prio(prio + (_kind >> 4)), const_value(0), const_value_f(0) {
		Init();
	}
	Node(int v) : kind(CONST), type(&constType), op_prio(0), const_value(v), const_value_f(v) {
		Init();
	}
	Node(bool isFloat, long vi, FLOAT vf) :
		kind(CONST), type(isFloat ? &constTypeF : &constType), const_value(vi), const_value_f(vf), op_prio(0) {
		Init();
	}
	Node(std::string _name) : kind(NAME), type(nullptr), op_prio(0), const_value(0), const_value_f(0) {
		Init();
		name = _name;
	}
	Node(Type *_type) : kind(CAST), type(_type), op_prio(prio + (CAST >> 4)), const_value(0), const_value_f(0) {
		Init();
	}
	void Init() {
		var = nullptr;
		c0 = c1 = c2 = nullptr;
	}
	void EmitAll(int regnumofs = 0);
	void Process();
	void ResolveName();
	template<typename F1, typename F2> void ProcessUnary(bool canFloat, F1 compute, F2 normal);
	template<typename F1, typename F2> void ProcessBin(bool canFloat, F1 compute, F2 normal);
	void ProcessSubst();
	void ProcessPointer(bool addf);
	void ProcessCall();
	void ProcessDot(Type *);
	void ProcessCond();
	void toshift(Node::Kind kind);
	void EmitLiteral(Obj::Inst inst, Type *dsttype, int p1);
	void EmitCall();
	void EmitPP();
	void EmitSubst();
	void EmitBin(Obj::Inst inst, Obj::Inst insti = Obj::NOP);
	void VarAccess(Obj::Inst inst, int num, int ofs);
	void StructAccess(Obj::Inst inst, bool store);
	void Emit(bool store = false);
	bool isUnary() const {
		return kind >= UNARY_START && kind <= UNARY_END;
	}
	bool isRight() const {
		return (kind >= RIGHT_START1 && kind <= RIGHT_END1) || (kind >= RIGHT_START2 && kind <= RIGHT_END2);
	}
	bool isSubst() const {
		return kind >= SUBST_START && kind <= SUBST_END;
	}
	Kind kind;
	Type *type;
	Var *var;
	int op_prio;
	long const_value;
	FLOAT const_value_f;
	Node *c0, *c1, *c2;
	std::string name;
	std::vector<Node *> func_arg;
	static void ImplicitCast(Type *dstType, Node *&src);
	static int inc_regnum();
	static int top() { return regnum; }
	static int second() { return regnum - 1; }
	static int GetUsedRegs() {
		int r = usedregs;
		usedregs = 0;
		return r;
	}
	static void StaticInit() {
		labelNum = prio = regnum = usedregs = stackAdjust = 0;
		stackArg.clear();
		constTypeF.attr._float = true;
	}
	static inline int labelNum, prio, regnum, usedregs, stackAdjust;
	static inline std::set<int> stackArg;
	static inline Type constType = Type(Type::VALUE, UNIT_SIZE);
	static inline Type constTypeF = Type(Type::VALUE, UNIT_SIZE);
	static inline std::map<Kind, Kind> substtbl = {
#define S(x) { x##_SUBST, x }
		S(PLUS), S(MINUS), S(MUL), S(DIV), S(REM), S(SL), S(SR), S(AND), S(OR), S(XOR)
#undef S
	};
};
