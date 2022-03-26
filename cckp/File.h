#include "types.h"

void FileClear();
void LoadSource(char *buf);
char *ErrorInfo(int *pos = nullptr);
char getcsub();
void _ungetc(char c);
char chkch();
void needch(char c);
void assertch(char c1, char c2);
std::string _gets();
void ungets(std::string &s);
std::string getword();
bool getvalue(long &vi, FLOAT &vf);
char escape();
void FilePrepro(const char *sourcename, std::string &dst);
void PushInc(std::string &pathname);
std::string getCurPath();
void AdjustLineNum(char c);

extern char (*_getc)();
extern char (*getch)();
