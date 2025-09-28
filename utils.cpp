#include "utils.hpp"

bool is_windows()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}
