#include <string>
#include <vector>

#define A_OUT		"a.srec"

void ReadLibPath(std::string path);
void Link(int argc, char *argv[], std::string &archive);

extern std::vector<std::string> libPath;
