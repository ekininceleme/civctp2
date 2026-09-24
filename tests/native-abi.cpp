#include "ctp2_config.h"
#include <cassert>
#include <cstring>
#include <string>

#if defined(__aarch64__) && defined(__arm__)
#error "The build must not redefine the compiler's 32-bit ARM macro on AArch64"
#endif

int main()
{
    const std::string shortName("Value");
    const std::string shortCopy(shortName);
    assert(shortCopy.size() == 5);
    assert(std::strcmp(shortCopy.c_str(), "Value") == 0);
    const std::string longName(96, 'x');
    const std::string longCopy(longName);
    assert(longCopy.size() == 96);
    assert(std::strlen(longCopy.c_str()) == 96);
    assert(longCopy == longName);
}
