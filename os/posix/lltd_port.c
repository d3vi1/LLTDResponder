#include "../../lltdResponder/lltdPort.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static void lltd_vlog(FILE *out, const char *prefix, const char *fmt, va_list args) {
    if (!out) {
        out = stderr;
    }
    if (prefix) {
        fputs(prefix, out);
    }
    vfprintf(out, fmt, args);
    fputc('\n', out);
    fflush(out);
}

void lltd_port_log_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lltd_vlog(stderr, "DEBUG: ", fmt, args);
    va_end(args);
}

void lltd_port_log_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lltd_vlog(stderr, "WARN: ", fmt, args);
    va_end(args);
}

uint64_t lltd_port_monotonic_seconds(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec;
    }
#endif
    return (uint64_t)time(NULL);
}

uint64_t lltd_port_monotonic_milliseconds(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
#endif
    return (uint64_t)time(NULL) * 1000ULL;
}
