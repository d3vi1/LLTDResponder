#include "../../../lltdResponder/lltdPort.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

void *lltd_port_memcpy(void *destination, const void *source, size_t num) {
    return memcpy(destination, source, num);
}

int lltd_port_memcmp(const void *lhs, const void *rhs, size_t num) {
    return memcmp(lhs, rhs, num);
}

void lltd_port_sleep_ms(uint32_t milliseconds) {
    Sleep(milliseconds);
}

int lltd_port_send_frame(void *iface_ctx, const void *frame, size_t frame_len) {
    (void)iface_ctx;
    (void)frame;
    (void)frame_len;
    return -1;
}

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

void *lltd_port_malloc(size_t size) {
    return malloc(size);
}

void lltd_port_free(void *ptr) {
    free(ptr);
}

void *lltd_port_memset(void *ptr, int value, size_t num) {
    return memset(ptr, value, num);
}

uint64_t lltd_port_monotonic_seconds(void) {
    return (uint64_t)(GetTickCount64() / 1000ULL);
}

uint64_t lltd_port_monotonic_milliseconds(void) {
    return (uint64_t)GetTickCount64();
}
