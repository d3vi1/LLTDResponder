#ifndef LLTD_TEST_SHIM_H
#define LLTD_TEST_SHIM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t mac[6];
    uint32_t characteristics_flags;
    uint32_t if_type;
    uint32_t ipv4_be;
    uint8_t ipv6[16];
    uint32_t link_speed_100bps;
    size_t mtu;

    bool wifi_supported;
    uint8_t wifi_mode;
    uint8_t bssid[6];
    uint8_t ssid[32];
    size_t ssid_len;
    uint16_t wifi_max_rate_0_5mbps;
    int8_t wifi_rssi_dbm;
    uint32_t wifi_phy_medium;
} lltd_test_iface_t;

void lltd_test_port_reset(void);
const void *lltd_test_port_last_frame(size_t *out_len);
int lltd_test_port_send_count(void);

#endif
