#include <map>
#include <string>

bool PreproActive();
void PreproDirective();
bool MacroIsDefined();
bool ExpandMacro(std::string &s, bool replace = false);
void preprocess(const char *path, bool depend, bool output);
void printDepend(const std::string &s, bool all);

extern std::map<std::string, std::string> macrosGlobal;
extern std::vector<std::string> includePath;
