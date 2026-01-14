/*
 * OS integration unit for protocol core automata.
 *
 * Keep all OS-specific conditionals out of `lltdResponder/`. The core is
 * compiled first, then we compile in the platform port implementation.
 */

#include "../lltdResponder/lltd_automata.c"

#if defined(__APPLE__)
#include "../os/darwin/lltd_port.c"
#elif defined(__linux__)
#include "../os/linux/lltd_port.c"
#elif defined(_WIN32)
#include "../os/windows/lltd_port.c"
#else
#include "../os/posix/lltd_port.c"
#endif

