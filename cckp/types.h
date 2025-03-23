#pragma once
#include <map>
#include <string>
#include <vector>

#define ISA64

#define INST_SIZE		4
#define INTEGER_SIZE	4
#ifdef ISA64
#define UNIT_SIZE		8
#define INT				long
#define FLOAT			double
#define FLOAT_FMT		"%lf"
#else
#define UNIT_SIZE		4
#define INT				int
#define FLOAT			float
#define FLOAT_FMT		"%f"
#endif

#define REGNUM_ZERO		0
#define REGNUM_BASE		1
#define REGNUM_END		8
#define REGNUM_RET		9
#define REGNUM_ARG		10
#define REGNUM_ARGEND	13
#define REGNUM_GP		14
#define REGNUM_SP		15

#define INT_VEC			4
#define INT_VEC_N		7
#define ORG_DATA		0x40000
#define GP_OFS			0x8000
#define MEM_AMOUNT		0x200000

struct Type;

struct Member {
	Member(Type *_type, int _ofs) : type(_type), ofs(_ofs) {}
	Type *type;
	int ofs;
};

struct Type {
	enum Kind { VALUE, POINTER, ARRAY, STRUCT, FUNC };
	Type(Kind _kind, int _size = 0) : kind(_kind), size(_size), next(nullptr) {}
	bool isPointer() const { return kind == POINTER || kind == ARRAY; }
	Kind kind;
	int size;
	Type *next;
	struct Attr {
		Attr() : _unsigned(false), _float(false), _static(false), _extern(false), _inline(false), _const(false), _varg(false) {}
		bool _unsigned, _float, _static, _extern, _inline, _const, _varg;
	} attr;
	std::vector<std::pair<std::string, Member>> members; // STRUCT
	std::vector<std::pair<std::string, Type *>> args; // FUNC
};

struct Var {
	Var() {}
	Var(Type *_type, int _ofs, bool _global, bool _datasection, bool _defined = false)
		: type(_type), ofs(_ofs), global(_global), datasection(_datasection), defined(_defined) {}
	Type *type;
	int ofs;
	bool global, datasection, defined;
};

struct Scope {
	Scope(int _ofs = 0) : ofs(_ofs) {}
	int ofs;
	std::map<std::string, long> consts;
	std::map<std::string, FLOAT> consts_f;
	std::map<std::string, Var> vars;
	std::map<std::string, Type *> types;
};

struct G {
	static inline std::map<std::string, Type *> funcs;
	static inline std::vector<Scope> scopeStack;
	static inline int dataOfs;
};

inline void Align(int &ofs, int size) {
	size = std::clamp(size, 1, UNIT_SIZE);
	int r = ofs % size;
	ofs += r > 0 ? size - r : -r;
}

inline int GetInterruptNum(const char *s) {
	return strncmp(s, "interrupt", 9) ? 0 : s[9] - '0';
}
