#include <cstdio>

#if _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

const char* sc_text()
{
	return "spell_check: ";
}

bool is_a_tty(FILE* fd)
{
#ifdef _WIN32
	return _isatty(_fileno(fd));
#else
	return isatty(fileno(fd));
#endif
}
