#if defined(_WIN32) || defined(_WIN64)
#include "region_windows.c"
#elif defined(__APPLE__)
// #include "region_apple.c"
#error Apple implementation TBD
#elif defined(__FreeBSD__)
// #include "region_freebsd.c"
#error FreeBSD implementation TBD
#elif defined(__linux__)
// #include "region_linux.c"
#error Linux implementation TBD
#else
#error Unsupported platform
#endif
