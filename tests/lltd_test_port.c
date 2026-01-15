#include "../lltdResponder/lltdPort.h"

#include "lltdTestShim.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static struct {
    void *frame;
    size_t frame_len;
    int send_count;
} g_test_tx;

static uint64_t g_monotonic_ms = 0;

void lltd_test_port_reset(void) {
    if (g_test_tx.frame) {
        lltd_port_free(g_test_tx.frame);
    }
    g_test_tx.frame = NULL;
    g_test_tx.frame_len = 0;
    g_test_tx.send_count = 0;
    g_monotonic_ms = 0;
}

const void *lltd_test_port_last_frame(size_t *out_len) {
    if (out_len) {
        *out_len = g_test_tx.frame_len;
    }
    return g_test_tx.frame;
}

int lltd_test_port_send_count(void) {
    return g_test_tx.send_count;
}

uint64_t lltd_port_monotonic_seconds(void) {
    return lltd_port_monotonic_milliseconds() / 1000ULL;
}

uint64_t lltd_port_monotonic_milliseconds(void) {
    g_monotonic_ms += 1;
    return g_monotonic_ms;
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

void *lltd_port_memcpy(void *destination, const void *source, size_t num) {
    return memcpy(destination, source, num);
}

int lltd_port_memcmp(const void *lhs, const void *rhs, size_t num) {
    return memcmp(lhs, rhs, num);
}

void lltd_port_sleep_ms(uint32_t milliseconds) {
    (void)milliseconds;
}

int lltd_port_send_frame(void *iface_ctx, const void *frame, size_t frame_len) {
    (void)iface_ctx;
    if (!frame || frame_len == 0) {
        return -1;
    }

    if (g_test_tx.frame) {
        lltd_port_free(g_test_tx.frame);
        g_test_tx.frame = NULL;
        g_test_tx.frame_len = 0;
    }

    g_test_tx.frame = lltd_port_malloc(frame_len);
    if (!g_test_tx.frame) {
        return -1;
    }
    lltd_port_memcpy(g_test_tx.frame, frame, frame_len);
    g_test_tx.frame_len = frame_len;
    g_test_tx.send_count++;
    return 0;
}

int lltd_port_get_mtu(void *iface_ctx, size_t *out_mtu) {
    if (!iface_ctx || !out_mtu) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    *out_mtu = iface->mtu ? iface->mtu : 1500;
    return 0;
}

int lltd_port_get_icon_image(void **out_data, size_t *out_size) {
    if (out_data) {
        *out_data = NULL;
    }
    if (out_size) {
        *out_size = 0;
    }
    return -1;
}

int lltd_port_get_friendly_name(void **out_data, size_t *out_size) {
    if (!out_data || !out_size) {
        return -1;
    }
    static const char name[] = "LLTD Test Host";
    size_t len = sizeof(name) - 1;
    char *buf = (char *)lltd_port_malloc(len + 1);
    if (!buf) {
        *out_data = NULL;
        *out_size = 0;
        return -1;
    }
    memcpy(buf, name, len + 1);
    *out_data = buf;
    *out_size = len;
    return 0;
}

size_t lltd_port_get_hostname(void *dst, size_t dst_len) {
    static const char name[] = "lltd-test-host";
    if (!dst || dst_len == 0) {
        return 0;
    }
    size_t len = sizeof(name) - 1;
    if (len > dst_len) {
        len = dst_len;
    }
    memcpy(dst, name, len);
    return len;
}

size_t lltd_port_get_support_url(void *dst, size_t dst_len) {
    static const char url[] = "https://example.invalid/support";
    if (!dst || dst_len == 0) {
        return 0;
    }
    size_t len = sizeof(url) - 1;
    if (len > dst_len) {
        len = dst_len;
    }
    memcpy(dst, url, len);
    return len;
}

int lltd_port_get_upnp_uuid(uint8_t out_uuid[16]) {
    if (!out_uuid) {
        return -1;
    }
    static const uint8_t uuid[16] = {
        0x12, 0x34, 0x56, 0x78,
        0x9a, 0xbc, 0xde, 0xf0,
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
    };
    memcpy(out_uuid, uuid, sizeof(uuid));
    return 0;
}

size_t lltd_port_get_hw_id(void *dst, size_t dst_len) {
    if (!dst || dst_len == 0) {
        return 0;
    }
    memset(dst, 0, dst_len);
    return 0;
}

int lltd_port_get_mac_address(void *iface_ctx, ethernet_address_t *out_mac) {
    if (!iface_ctx || !out_mac) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    memcpy(out_mac->a, iface->mac, sizeof(out_mac->a));
    return 0;
}

uint32_t lltd_port_get_characteristics_flags(void *iface_ctx) {
    if (!iface_ctx) {
        return 0;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    return iface->characteristics_flags;
}

int lltd_port_get_if_type(void *iface_ctx, uint32_t *out_if_type) {
    if (!iface_ctx || !out_if_type) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    *out_if_type = iface->if_type;
    return 0;
}

int lltd_port_get_ipv4_address(void *iface_ctx, uint32_t *out_ipv4_be) {
    if (!iface_ctx || !out_ipv4_be) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    *out_ipv4_be = iface->ipv4_be;
    return 0;
}

int lltd_port_get_ipv6_address(void *iface_ctx, uint8_t out_ipv6[16]) {
    if (!iface_ctx || !out_ipv6) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    memcpy(out_ipv6, iface->ipv6, 16);
    return 0;
}

int lltd_port_get_link_speed_100bps(void *iface_ctx, uint32_t *out_speed_100bps) {
    if (!iface_ctx || !out_speed_100bps) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    *out_speed_100bps = iface->link_speed_100bps;
    return 0;
}

int lltd_port_get_wifi_mode(void *iface_ctx, uint8_t *out_mode) {
    if (!iface_ctx || !out_mode) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    if (!iface->wifi_supported) {
        return -1;
    }
    *out_mode = iface->wifi_mode;
    return 0;
}

int lltd_port_get_bssid(void *iface_ctx, uint8_t out_bssid[6]) {
    if (!iface_ctx || !out_bssid) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    if (!iface->wifi_supported) {
        return -1;
    }
    memcpy(out_bssid, iface->bssid, 6);
    return 0;
}

size_t lltd_port_get_ssid(void *iface_ctx, void *dst, size_t dst_len) {
    if (!iface_ctx || !dst || dst_len == 0) {
        return 0;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    if (!iface->wifi_supported) {
        return 0;
    }
    size_t len = iface->ssid_len;
    if (len > dst_len) {
        len = dst_len;
    }
    memcpy(dst, iface->ssid, len);
    return len;
}

int lltd_port_get_wifi_max_rate_0_5mbps(void *iface_ctx, uint16_t *out_units_0_5mbps) {
    if (!iface_ctx || !out_units_0_5mbps) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    if (!iface->wifi_supported) {
        return -1;
    }
    *out_units_0_5mbps = iface->wifi_max_rate_0_5mbps;
    return 0;
}

int lltd_port_get_wifi_rssi_dbm(void *iface_ctx, int8_t *out_rssi_dbm) {
    if (!iface_ctx || !out_rssi_dbm) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    if (!iface->wifi_supported) {
        return -1;
    }
    *out_rssi_dbm = iface->wifi_rssi_dbm;
    return 0;
}

int lltd_port_get_wifi_phy_medium(void *iface_ctx, uint32_t *out_phy_medium) {
    if (!iface_ctx || !out_phy_medium) {
        return -1;
    }
    const lltd_test_iface_t *iface = (const lltd_test_iface_t *)iface_ctx;
    if (!iface->wifi_supported) {
        return -1;
    }
    *out_phy_medium = iface->wifi_phy_medium;
    return 0;
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
    lltd_vlog(stderr, "TEST-DEBUG: ", fmt, args);
    va_end(args);
}

void lltd_port_log_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lltd_vlog(stderr, "TEST-WARN: ", fmt, args);
    va_end(args);
}

