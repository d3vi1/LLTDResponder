#ifndef LLTD_RESPONDER_PORT_API_H
#define LLTD_RESPONDER_PORT_API_H

#include <stdint.h>

uint64_t lltd_port_monotonic_seconds(void);
uint64_t lltd_port_monotonic_milliseconds(void);

void lltd_port_log_debug(const char *fmt, ...);
void lltd_port_log_warning(const char *fmt, ...);

#endif
