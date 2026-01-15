#ifndef LLTD_RESPONDER_DAEMON_CORE_H
#define LLTD_RESPONDER_DAEMON_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "lltdProtocol.h"

typedef struct lltd_iface {
    void *ctx; /* passed through to lltd_port_send_frame() */
    ethernet_address_t mac;
    size_t mtu;
} lltd_iface;

int lltd_daemon_handle_frame(const lltd_iface *iface,
                             const void *frame,
                             size_t frame_len);

#endif
