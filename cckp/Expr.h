#include "types.h"

struct Node;

void ExprInit(bool prepro);
Type *BaseType();
std::string ParseType(Type *&top);
char ExprContext(bool dump, bool commaInhibit = false, Type *type = nullptr);
Node *ExprContents();
long ConstExpr(bool commaInhibit = false);
std::string StringLiteral(int *np);
void Align(int &ofs, int size = UNIT_SIZE);
