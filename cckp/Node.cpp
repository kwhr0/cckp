#include "Node.h"
#include "Error.h"

void Node::ImplicitCast(Type *dstType, Node *&src) {
	if (dstType->kind != Type::VALUE || src->type->kind != Type::VALUE || dstType->attr._float == src->type->attr._float) return;
	if (src->kind == Node::CONST) {
		if (src->type->attr._float) src->const_value = src->const_value_f;
		else src->const_value_f = src->const_value;
		src->type = new Type(*dstType);
		src->type->attr._float = dstType->attr._float;
	}
	else {
		Node *node = new Node(dstType);
		node->c0 = src;
		src = node;
	}
}

template<typename F1, typename F2> void Node::ProcessUnary(bool canFloat, F1 compute, F2 normal) {
	c0->Process();
	if (!canFloat && c0->type->attr._float) Error(ERR_CANT_FLOAT);
	if (c0->type->kind == Type::STRUCT) Error(ERR_CANT_STRUCT);
	type = new Type(Type::VALUE, c0->type->size);
	type->attr._unsigned = c0->type->attr._unsigned;
	type->attr._float = c0->type->attr._float;
	if (c0->kind == Node::CONST) {
		kind = Node::CONST;
		type = &constType;
		const_value = compute(c0->const_value);
		if (canFloat) const_value_f = compute(c0->const_value_f);
	}
	else normal();
}

template<typename F1, typename F2> void Node::ProcessBin(bool canFloat, F1 compute, F2 normal) {
	c0->Process();
	c1->Process();
	if (!canFloat && (c0->type->attr._float || c1->type->attr._float)) Error(ERR_CANT_FLOAT);
	if (c0->type->kind == Type::STRUCT || c1->type->kind == Type::STRUCT) Error(ERR_CANT_STRUCT);
	type = new Type(Type::VALUE, std::max(c0->type->size, c1->type->size));
	type->attr._unsigned = (c0->type->size >= UNIT_SIZE && c0->type->attr._unsigned) ||
							(c1->type->size >= UNIT_SIZE && c1->type->attr._unsigned);
	type->attr._float = c0->type->attr._float || c1->type->attr._float;
	if (c0->kind == Node::CONST && c1->kind == Node::CONST) {
		kind = Node::CONST;
		const_value = compute(c0->const_value, c1->const_value);
		if (canFloat) const_value_f = compute(c0->const_value_f, c1->const_value_f);
	}
	else {
		ImplicitCast(type, c0);
		ImplicitCast(type, c1);
		normal();
	}
}

void Node::toshift(Node::Kind _kind) {
	if (c0->type->attr._float || c1->kind != Node::CONST) return;
	int i;
	for (i = 1; i < 8 * UNIT_SIZE && c1->const_value > (1L << i); i++)
		;
	if (c1->const_value == (1L << i)) c1->const_value = (kind = _kind) == Node::AND ? c1->const_value - 1 : i;
}

void Node::ProcessPointer(bool addf) {
	if (!c0->type->isPointer()) return;
	type = c0->type;
	if (c1->type->isPointer()) {
		if (addf) Error(ERR_POINTER_ARITH);
		if (type->next->size > 1) {
			Node *node = new Node(Node::MINUS);
			node->type = &constType;
			node->c0 = c0;
			node->c1 = c1;
			c0 = node;
			c1 = new Node(type->next->size);
			kind = DIV;
			type = &constType;
			toshift(Node::SR);
		}
		type = &constType;
	}
	else if (type->next->size > 1) {
		if (c1->kind == Node::CONST) c1->const_value *= type->next->size;
		else {
			Node *node = new Node(Node::MUL);
			node->type = c1->type;
			node->c0 = c1;
			node->c1 = new Node(type->next->size);
			c1 = node;
			c1->toshift(Node::SL);
		}
	}
}

void Node::ProcessSubst() {
	if (substtbl.count(kind)) {
		Node *node = new Node(*this);
		node->kind = substtbl[kind];
		kind = SUBST;
		c1 = node;
	}
	c0->Process();
	if (c0->type->attr._const) Error(ERR_CANT_CONST);
	c1->Process();
	ImplicitCast(c0->type, c1);
	type = c0->type;
}

void Node::ProcessCall() {
	c0->Process();
	Type *type1;
	if (c0->kind == Node::NAME) {
		if (!G::funcs.count(c0->name)) Error(ERR_UNDEF, c0->name.c_str());
		type1 = G::funcs[c0->name];
	}
	else {
		if (c0->kind == Node::POINTER) c0 = c0->c0;
		type1 = c0->type;
	}
	type = type1->next;
	if (type == nullptr) Error(ERR_NO_FUNC);
	int i = 0;
	for (Node *&node : func_arg) {
		node->Process();
		if (i < type1->args.size()) ImplicitCast(type1->args[i++].second, node);
	}
}

void Node::ProcessDot(Type *type1) {
	if (type1->kind != Type::STRUCT) Error(ERR_NOT_STRUCT);
	auto it = std::find_if(begin(type1->members), end(type1->members), [&](auto m) { return m.first == c1->name; });
	if (it == end(type1->members)) Error(ERR_NO_MEMBER, c1->name.c_str());
	type = it->second.type;
	var = c0->var;
	const_value = it->second.ofs;
	name = c1->name;
}

void Node::ProcessCond() {
	c0->Process();
	if (c0->kind == Node::CONST) {
		if (!(c0->type->attr._float ? (long)c0->const_value_f : c0->const_value)) c1 = c2;
		c1->Process();
		*this = *c1;
	}
	else {
		c1->Process();
		c2->Process();
		type = new Type(*c1->type); // incomplete type check
		type->size = std::max(c1->type->kind == Type::VALUE ? c1->type->size : UNIT_SIZE,
							  c2->type->kind == Type::VALUE ? c2->type->size : UNIT_SIZE);
		type->attr._unsigned = c1->type->attr._unsigned || c2->type->attr._unsigned;
		type->attr._float = c1->type->attr._float || c2->type->attr._float;
		ImplicitCast(type, c1);
		ImplicitCast(type, c2);
	}
}

void Node::ResolveName() {
	for (auto i = rbegin(G::scopeStack); i != rend(G::scopeStack); i++) {
		if (i->vars.count(name)) {
			kind = Node::VAR;
			var = &i->vars[name];
			type = var->type;
			if (!type->attr._const) return;
		}
		if (i->consts.count(name)) {
			kind = Node::CONST;
			const_value = i->consts[name];
			if (i->consts_f.count(name)) const_value_f = i->consts_f[name];
			type = &constType;
			return;
		}
	}
	if (G::funcs.count(name)) type = G::funcs[name];
	else if (!Obj::labels.count(name)) Error(ERR_UNDEF, name.c_str());
}

void Node::Process() {
	Type *type1;
	Node *node;
	switch (kind) {
		case Node::POST_PLUS: case Node::POST_MINUS: case Node::PRE_PLUS: case Node::PRE_MINUS:
			c0->Process();
			type = c0->type;
			const_value_f = const_value = (kind == Node::POST_PLUS || kind == Node::PRE_PLUS ? 1 : -1) *
				(c0->type->isPointer() ? c0->type->next->size : 1);
			break;
		case Node::ARRAY:
			node = new Node(Node::PLUS);
			node->c0 = c0;
			node->c1 = c1;
			kind = Node::POINTER;
			c0 = node;
			c1 = nullptr;
			// no break
		case Node::POINTER:
			c0->Process();
			if (!(type = c0->type->next)) Error(ERR_NOT_POINTER);
			break;
		case Node::ADDRESS:
			c0->Process();
			type1 = new Type(Type::POINTER, UNIT_SIZE);
			type1->next = c0->type;
			type = type1;
			name = c0->name;
			break;
		case Node::ARROW:
			c0->Process();
			if (!c0->type->next) Error(ERR_NOT_POINTER);
			ProcessDot(c0->type->next);
			break;
		case Node::DOT:
			c0->Process();
			ProcessDot(c0->type);
			break;
		case Node::CALL:
			ProcessCall();
			break;
		case Node::SMINUS:
			ProcessUnary(true, [](auto a) { return -a; }, []{});
			break;
		case Node::COM:
			ProcessUnary(false, [](long a) { return ~a; }, []{});
			break;
		case Node::NOT:
			ProcessUnary(true, [](auto a) { return !a; }, [&] {
				type->size = UNIT_SIZE;
				type->attr._float = false;
			});
			break;
		case Node::CAST:
			c0->Process(); // don't call ProcessUnary() to keep casted type
			if (type->kind == Type::VALUE && c0->kind == CONST) {
				kind = CONST;
				if (type->attr._float) const_value_f = c0->const_value;
				else const_value = c0->const_value_f;
			}
			break;
		case Node::MUL:
			ProcessBin(true, [](auto a, auto b) { return a * b; }, [&] {
				if (c0->kind == Node::CONST) std::swap(c0, c1);
				toshift(Node::SL);
			});
			break;
		case Node::DIV:
			ProcessBin(true, [](auto a, auto b) { return a / b; }, [&] { toshift(Node::SR); });
			break;
		case Node::REM:
			ProcessBin(false, [](long a, long b) { return a % b; }, [&] { toshift(Node::AND); });
			break;
		case Node::PLUS:
			ProcessBin(true, [](auto a, auto b) { return a + b; }, [&] {
				if (c0->kind == Node::CONST || c1->type->isPointer()) std::swap(c0, c1);
				ProcessPointer(true);
			});
			break;
		case Node::MINUS:
			ProcessBin(true, [](auto a, auto b) { return a - b; }, [&] { ProcessPointer(false); });
			break;
		case Node::SL:
			ProcessBin(false, [](long a, long b) { return a << b; }, [&] { type = c0->type; });
			break;
		case Node::SR:
			ProcessBin(false, [](long a, long b) { return a >> b; }, [&] { type = c0->type; });
			break;
		case Node::LT:
			ProcessBin(true, [](auto a, auto b) { return a < b; }, []{});
			break;
		case Node::LE:
			ProcessBin(true, [](auto a, auto b) { return a <= b; }, []{});
			break;
		case Node::GT:
			ProcessBin(true, [](auto a, auto b) { return a > b; }, []{});
			break;
		case Node::GE:
			ProcessBin(true, [](auto a, auto b) { return a >= b; }, []{});
			break;
		case Node::EQ:
			ProcessBin(true, [](auto a, auto b) { return a == b; }, []{});
			break;
		case Node::NE:
			ProcessBin(true, [](auto a, auto b) { return a != b; }, []{});
			break;
		case Node::AND:
			ProcessBin(false, [](long a, long b) { return a & b; }, []{});
			break;
		case Node::XOR:
			ProcessBin(false, [](long a, long b) { return a ^ b; }, []{});
			break;
		case Node::OR:
			ProcessBin(false, [](long a, long b) { return a | b; }, []{});
			break;
		case Node::ANDAND:
			ProcessBin(true, [](auto a, auto b) { return a && b; }, [&] {
				if (c0->kind == Node::CONST) {
					if (c0->type->attr._float ? (long)c0->const_value_f : c0->const_value) kind = Node::COND1;
					else {
						kind = Node::CONST;
						const_value_f = const_value = 0;
					}
				}
			});
			break;
		case Node::OROR:
			ProcessBin(true, [](auto a, auto b) { return a || b; }, [&] {
				if (c0->kind == Node::CONST) {
					if (c0->type->attr._float ? (long)c0->const_value_f : c0->const_value) {
						kind = Node::CONST;
						const_value_f = const_value = 1;
					}
					else kind = Node::COND1;
				}
			});
			break;
		case Node::SUBST:
		case Node::PLUS_SUBST: case Node::MINUS_SUBST: case Node::MUL_SUBST: case Node::DIV_SUBST: case Node::REM_SUBST:
		case Node::SL_SUBST: case Node::SR_SUBST:
		case Node::AND_SUBST: case Node::OR_SUBST: case Node::XOR_SUBST:
			ProcessSubst();
			break;
		case Node::COND:
			ProcessCond();
			break;
		case Node::COMMA:
			c0->Process();
			c1->Process();
			type = c1->type;
			break;
		case Node::NAME:
			ResolveName();
			if (kind == Node::NAME && !type) Error(ERR_UNDEF, name.c_str());
			break;
		case Node::CONST:
			break;
		default:
			if (!c0) break;
			if (c1) ProcessBin(true, [](auto, auto) { return 0; }, []{});
			else ProcessUnary(true, [](auto) { return 0; }, []{});
			break;
	}
}

int Node::inc_regnum() {
	int r = regnum++;
	usedregs |= 1 << r;
	if (r > REGNUM_END) Error(ERR_COMPLEX);
	return r;
}

static bool checkType(Type *formal, Type *actual) { // incomplete
	return formal->kind == actual->kind || formal->kind == Type::POINTER;
}

void Node::EmitCall() {
	if (c0->kind != Node::NAME) c0->Emit();
	Type *type1 = c0->type->kind == Type::POINTER ? c0->type->next : c0->type;
	if (type1->attr._varg ? func_arg.size() < type1->args.size() : func_arg.size() != type1->args.size()) Error(ERR_ARG);
	int ofs = 0, adjust = 0, i = 0, r = REGNUM_ARG, regnum_save = regnum;
	bool retStruct = type1->next->kind == Type::STRUCT;
	if (retStruct) ofs = adjust = type1->next->size;
	adjust += UNIT_SIZE * func_arg.size();
	if (adjust) Obj::code.emplace_back(Obj::ADDI, REGNUM_SP, -adjust);
	size_t save = std::distance(begin(Obj::code), end(Obj::code));
	std::vector<Obj> argregs;
	for (Node *node : func_arg) {
		if (i < type1->args.size() && !checkType(type1->args[i].second, node->type)) Error(ERR_ARG_TYPE, i + 1);
		if (node->type->kind == Type::STRUCT) {
			Obj::code.emplace_back(Obj::LEA, inc_regnum(), ofs, REGNUM_SP, 0, "arg", Obj::FIX);
			node->VarAccess(Obj::LEA, inc_regnum(), node->var->ofs);
			inc_regnum();
			Obj::_memcpy(second(), node->type->size);
			regnum -= 3;
			ofs += node->type->size;
		}
		else {
			int last = regnum;
			node->Emit();
			if (i < type1->args.size() && r <= REGNUM_ARGEND) {
				stackArg.insert(ofs + UNIT_SIZE);
				usedregs |= 1 << r;
				argregs.emplace_back(Obj::MOV, r++, last);
			}
			else Obj::code.emplace_back(Obj::ST, regnum = last, ofs, REGNUM_SP, UNIT_SIZE, "arg", Obj::FIX);
			ofs += std::max(node->type->kind == Type::VALUE ? node->type->size : 0, UNIT_SIZE);
		}
		i++;
	}
	Obj::code.insert(end(Obj::code), begin(argregs), end(argregs));
	regnum = regnum_save;
	Obj::AdjustOffset(save, [&](bool) { return adjust; });
	if (c0->type->kind == Type::POINTER) {
		regnum--;
		Obj::code.emplace_back(Obj::CALLV, top());
	}
	else if (Obj::inlinecode.count(c0->name)) {
		Obj::code.emplace_back(Obj::COMMENT, "expand inline: " + c0->name);
		++labelNum;
		Obj::code.emplace_back(Obj::ADDI, REGNUM_SP, -UNIT_SIZE);
		for (Obj &o : Obj::inlinecode[c0->name]) Obj::code.emplace_back(o).suffix = labelNum;
		Obj::code.emplace_back(Obj::ADDI, REGNUM_SP, UNIT_SIZE);
	}
	else Obj::code.emplace_back(Obj::CALL, c0->name.c_str());
	if (retStruct) stackAdjust += adjust;
	else if (adjust) Obj::code.emplace_back(Obj::ADDI, REGNUM_SP, adjust);
	if (type1->next->size) Obj::code.emplace_back(Obj::MOV, inc_regnum(), REGNUM_RET);
}

void Node::EmitSubst() {
	if (c0->type->kind == Type::STRUCT) {
		if (c1->type->kind != Type::STRUCT || c0->type->size != c1->type->size) Error(ERR_SUBST);
		if (c0->kind == Node::VAR) c0->VarAccess(Obj::LEA, inc_regnum(), c0->var->ofs);
		else c0->Emit();
		if (c1->kind == Node::VAR) c1->VarAccess(Obj::LEA, inc_regnum(), c1->var->ofs);
		else c1->Emit();
		inc_regnum();
		Obj::_memcpy(second(), c0->type->size);
		regnum -= 2;
		return;
	}
	if (c0->kind != Node::VAR && c0->kind != Node::POINTER && c0->kind != Node::DOT && c0->kind != Node::ARROW) Error(ERR_SUBST);
	if (c1->type->kind == Type::VALUE && !c1->type->size) Error(ERR_SUBST);
	c1->Emit();
	c0->Emit(true);
}

void Node::EmitLiteral(Obj::Inst inst, Type *dsttype, int p1) {
	if (dsttype->attr._float) {
		FLOAT f = type->attr._float ? const_value_f : (FLOAT)const_value;
		char s[256];
		sprintf(s, FLOAT_FMT, f);
		INT v;
		(FLOAT &)v = f;
		Obj::code.emplace_back(inst, s, p1, v);
	}
	else Obj::code.emplace_back(inst, "", p1, type->attr._float ? (long)const_value_f : const_value);
}

void Node::EmitPP() {
	auto incdec = [&] {
		if (type->attr._float) {
			EmitLiteral(Obj::LI, type, inc_regnum());
			regnum = second();
			Obj::code.emplace_back(Obj::ADDF, second(), top());
		}
		else Obj::code.emplace_back(Obj::ADDI, second(), const_value);
	};
	c0->Emit();
	if (c0->kind == Node::VAR) {
		incdec();
		c0->VarAccess(Obj::ST, second(), c0->var->ofs);
	}
	else {
		if (c0->kind != Node::POINTER && c0->kind != Node::DOT && c0->kind != Node::ARROW) Error(ERR_SUBST);
		if (!Obj::code.back().IsLoad()) Fatal(FATAL_INTERNAL);
		Obj::code.back().reg1++; // fix load register to use pointer
		inc_regnum();
		incdec();
		regnum = second();
		Obj::code.emplace_back(Obj::ST, top(), 0, second(), c0->type->size);
		Obj::code.emplace_back(Obj::MOV, second(), top());
	}
	if (kind == Node::POST_PLUS || kind == Node::POST_MINUS) {
		const_value_f = const_value = -const_value;
		incdec();
	}
}

void Node::EmitBin(Obj::Inst inst, Obj::Inst insti) {
	long v = c1->const_value;
	if (insti != Obj::NOP && c1->kind == Node::CONST &&
		(insti == Obj::LI || insti == Obj::ADDI ? v >= -32768 && v <= 32767 : v >= 0 && v <= 65535)) {
		c0->Emit();
		if (v) Obj::code.emplace_back(insti, second(), v);
	}
	else {
		c0->Emit();
		c1->Emit();
		regnum = second();
		Obj::code.emplace_back(inst, second(), top());
	}
}

void Node::VarAccess(Obj::Inst inst, int num, int ofs) {
	Obj::code.emplace_back(inst, num, var->global ? ofs - GP_OFS : ofs,
						   var->global ? REGNUM_GP : REGNUM_SP, type->size, name,
						   (type->attr._extern ? Obj::EXT : 0) |
						   (!var->global && ofs > 0 ? Obj::ARG : 0) |
						   (var->datasection ? Obj::DSEC : 0));
}

void Node::StructAccess(Obj::Inst inst, bool store) {
	if (c0->kind == Node::VAR) c0->VarAccess(inst, inc_regnum(), var->ofs);
	else c0->Emit();
	if (const_value) Obj::code.emplace_back(Obj::ADDI, second(), const_value, 0, 0, name);
	if (type->kind == Type::ARRAY || type->kind == Type::STRUCT) return;
	if (store) {
		regnum = second();
		Obj::code.emplace_back(Obj::ST, second(), 0, top(), type->size);
	}
	else Obj::code.emplace_back(type->attr._unsigned ? Obj::LDU : Obj::LD, second(), 0, second(), type->size);
}

void Node::Emit(bool store) {
	int l;
	switch (kind) {
		case Node::POST_PLUS: case Node::POST_MINUS: case Node::PRE_PLUS: case Node::PRE_MINUS:
			EmitPP();
			break;
		case Node::ARROW:
			StructAccess(Obj::LD, store);
			break;
		case Node::DOT:
			StructAccess(Obj::LEA, store);
			break;
		case Node::POINTER:
			c0->Emit();
			if (store) {
				regnum = second();
				Obj::code.emplace_back(Obj::ST, second(), 0, top(), type->size);
			}
			else if (type->kind != Type::STRUCT && type->kind != Type::ARRAY)
				Obj::code.emplace_back(type->attr._unsigned ? Obj::LDU : Obj::LD, second(), 0, second(), type->size);
			break;
		case Node::ADDRESS:
			c0->Emit();
			if (c0->type->kind == Type::STRUCT || c0->type->kind == Type::ARRAY) break;
			if (!Obj::code.back().IsLoad()) Error(ERR_ADDRESS);
			if (Obj::code.back().reg2 == REGNUM_SP) stackArg.insert((int)Obj::code.back().imm);
			Obj::code.back().inst = Obj::LEA;
			break;
		case Node::NOT:
			c0->Emit();
			Obj::code.emplace_back(c0->type->attr._float ? Obj::SEQF : Obj::SEQ, second(), REGNUM_ZERO);
			break;
		case Node::COM:
			c0->Emit();
			Obj::code.emplace_back(Obj::COM, second(), type->size);
			break;
		case Node::SMINUS:
			c0->Emit();
			Obj::code.emplace_back(type->attr._float ? Obj::NEGF : Obj::NEG, second(), type->size);
			break;
		case Node::CAST:
			c0->Emit();
			if (!c0->type->attr._float && type->attr._float)
				Obj::code.emplace_back(Obj::CVTF, second(), c0->type->size);
			else if (c0->type->attr._float && !type->attr._float)
				Obj::code.emplace_back(Obj::CVTI, second(), c0->type->size);
			break;
		case Node::SPLUS:
			c0->Emit();
			break;
		case Node::CALL:
			EmitCall();
			break;
		case Node::PLUS:
			if (type->attr._float) EmitBin(Obj::ADDF);
			else EmitBin(Obj::ADD, Obj::ADDI);
			break;
		case Node::MINUS:
			if (type->attr._float) EmitBin(Obj::SUBF);
			else EmitBin(Obj::SUB, Obj::SUBI);
			break;
		case Node::MUL:
			EmitBin(type->attr._float ? Obj::MULF : type->attr._unsigned ? Obj::MULU : Obj::MUL);
			break;
		case Node::DIV:
			EmitBin(type->attr._float ? Obj::DIVF : type->attr._unsigned ? Obj::DIVU : Obj::DIV);
			break;
		case Node::REM:
			EmitBin(type->attr._unsigned ? Obj::REMU : Obj::REM);
			break;
		case Node::SL:
			EmitBin(Obj::SLLV, Obj::SLL);
			break;
		case Node::SR:
			EmitBin(type->attr._unsigned ? Obj::SRLV : Obj::SRAV, type->attr._unsigned ? Obj::SRL : Obj::SRA);
			break;
		case Node::EQ:
			EmitBin(type->attr._float ? Obj::SEQF : Obj::SEQ);
			break;
		case Node::NE:
			EmitBin(type->attr._float ? Obj::SNEF : Obj::SNE);
			break;
		case Node::GT:
			EmitBin(type->attr._float ? Obj::SGTF : type->attr._unsigned ? Obj::SGTU : Obj::SGT);
			break;
		case Node::GE:
			EmitBin(type->attr._float ? Obj::SGEF : type->attr._unsigned ? Obj::SGEU : Obj::SGE);
			break;
		case Node::LT:
			EmitBin(type->attr._float ? Obj::SLTF : type->attr._unsigned ? Obj::SLTU : Obj::SLT);
			break;
		case Node::LE:
			EmitBin(type->attr._float ? Obj::SLEF : type->attr._unsigned ? Obj::SLEU : Obj::SLE);
			break;
		case Node::AND:
			EmitBin(Obj::AND, Obj::ANDI);
			break;
		case Node::OR:
			EmitBin(Obj::OR, Obj::ORI);
			break;
		case Node::XOR:
			EmitBin(Obj::XOR, Obj::XORI);
			break;
		case Node::ANDAND:
			c0->Emit();
			l = ++labelNum;
			Obj::code.emplace_back(c0->type->attr._float ? Obj::BEQF : Obj::BEQ, "andand_tail", second(), REGNUM_ZERO, l);
			regnum = second();
			c1->Emit();
			Obj::code.emplace_back(c1->type->attr._float ? Obj::SNEF : Obj::SNE, second(), REGNUM_ZERO);
			Obj::code.emplace_back(Obj::LABEL, "andand_tail", l);
			break;
		case Node::OROR:
			c0->Emit();
			l = ++labelNum;
			Obj::code.emplace_back(c0->type->attr._float ? Obj::BNEF : Obj::BNE, "oror_tail", second(), REGNUM_ZERO, l);
			regnum = second();
			c1->Emit();
			Obj::code.emplace_back(Obj::LABEL, "oror_tail", l);
			Obj::code.emplace_back(c1->type->attr._float ? Obj::SNEF : Obj::SNE, second(), REGNUM_ZERO);
			break;
		case Node::COND:
			c0->Emit();
			l = ++labelNum;
			Obj::code.emplace_back(Obj::BEQ, "colon", c0->second(), REGNUM_ZERO, l);
			regnum = c0->second();
			c1->Emit();
			Obj::code.emplace_back(Obj::BRA, "cond_tail", l);
			Obj::code.emplace_back(Obj::LABEL, "colon", l);
			regnum = c1->second();
			c2->Emit();
			Obj::code.emplace_back(Obj::LABEL, "cond_tail", l);
			break;
		case Node::SUBST:
			EmitSubst();
			break;
		case Node::CONST:
			EmitLiteral(Obj::LI, type, inc_regnum());
			break;
		case Node::NAME:
			Obj::code.emplace_back(Obj::LIL, name, inc_regnum()); // literal
			break;
		case Node::COMMA:
			c0->Emit();
			regnum = second();
			c1->Emit();
			break;
		case Node::COND1:
			c1->Emit();
			Obj::code.emplace_back(Obj::SNE, second(), REGNUM_ZERO);
			break;
		case Node::VAR:
			if (store) VarAccess(Obj::ST, second(), var->ofs);
			else VarAccess(type->kind == Type::ARRAY || type->kind == Type::STRUCT ?
						   Obj::LEA : type->attr._unsigned ? Obj::LDU : Obj::LD, inc_regnum(), var->ofs);
			break;
		default:
			break;
	}
}

void Node::EmitAll(int regnumofs) {
	regnum = REGNUM_BASE + regnumofs;
	stackAdjust = 0;
	Emit();
	if (stackAdjust) Obj::code.emplace_back(Obj::ADDI, REGNUM_SP, stackAdjust);
}
