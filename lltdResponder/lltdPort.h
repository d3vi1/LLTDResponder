#ifndef LLTD_RESPONDER_PORT_API_H
#define LLTD_RESPONDER_PORT_API_H

#include <stddef.h>
#include <stdint.h>

uint64_t lltd_port_monotonic_seconds(void);
uint64_t lltd_port_monotonic_milliseconds(void);

void *lltd_port_malloc(size_t size);
void lltd_port_free(void *ptr);
void *lltd_port_memset(void *ptr, int value, size_t num);

void lltd_port_log_debug(const char *fmt, ...);
void lltd_port_log_warning(const char *fmt, ...);

#endif
