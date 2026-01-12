// Force include header for MinGW builds
// Include Windows headers first, then undef byte to avoid conflict with std::byte
#ifdef __MINGW32__
#include <windows.h>
#undef byte
#endif
