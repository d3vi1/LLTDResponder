#include "../../lltdResponder/lltd_port.h"

#include <mach/mach_time.h>
#include <stdarg.h>
#include <stdio.h>

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

static uint64_t lltd_mach_now_ns(void) {
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t now = mach_absolute_time();
    return (now * timebase.numer) / timebase.denom;
}

uint64_t lltd_port_monotonic_seconds(void) {
    return lltd_mach_now_ns() / 1000000000ULL;
}

uint64_t lltd_port_monotonic_milliseconds(void) {
    return lltd_mach_now_ns() / 1000000ULL;
}

