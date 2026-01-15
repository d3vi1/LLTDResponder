#include "../../lltdResponder/lltdPort.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void *lltd_port_memcpy(void *destination, const void *source, size_t num) {
    return memcpy(destination, source, num);
}

int lltd_port_memcmp(const void *lhs, const void *rhs, size_t num) {
    return memcmp(lhs, rhs, num);
}

void lltd_port_sleep_ms(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)(milliseconds / 1000U);
    ts.tv_nsec = (long)((milliseconds % 1000U) * 1000000UL);
    nanosleep(&ts, NULL);
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

size_t lltd_port_get_hostname(void *dst, size_t dst_len) {
    if (!dst || dst_len == 0) {
        return 0;
    }
    char hostname[256];
    hostname[0] = '\0';
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return 0;
    }
    size_t len = strnlen(hostname, sizeof(hostname));
    if (len > dst_len) {
        len = dst_len;
    }
    memcpy(dst, hostname, len);
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
    (void)out_uuid;
    return -1;
}

size_t lltd_port_get_hw_id(void *dst, size_t dst_len) {
    (void)dst;
    (void)dst_len;
    return 0;
}

int lltd_port_get_mac_address(void *iface_ctx, ethernet_address_t *out_mac) {
    (void)iface_ctx;
    (void)out_mac;
    return -1;
}

uint32_t lltd_port_get_characteristics_flags(void *iface_ctx) {
    (void)iface_ctx;
    return 0;
}

int lltd_port_get_if_type(void *iface_ctx, uint32_t *out_if_type) {
    (void)iface_ctx;
    if (out_if_type) {
        *out_if_type = 0;
    }
    return -1;
}

int lltd_port_get_ipv4_address(void *iface_ctx, uint32_t *out_ipv4_be) {
    (void)iface_ctx;
    if (out_ipv4_be) {
        *out_ipv4_be = 0;
    }
    return -1;
}

int lltd_port_get_ipv6_address(void *iface_ctx, uint8_t out_ipv6[16]) {
    (void)iface_ctx;
    if (out_ipv6) {
        memset(out_ipv6, 0, 16);
    }
    return -1;
}

int lltd_port_get_link_speed_100bps(void *iface_ctx, uint32_t *out_speed_100bps) {
    (void)iface_ctx;
    if (out_speed_100bps) {
        *out_speed_100bps = 0;
    }
    return -1;
}

int lltd_port_get_wifi_mode(void *iface_ctx, uint8_t *out_mode) {
    (void)iface_ctx;
    (void)out_mode;
    return -1;
}

int lltd_port_get_bssid(void *iface_ctx, uint8_t out_bssid[6]) {
    (void)iface_ctx;
    (void)out_bssid;
    return -1;
}

size_t lltd_port_get_ssid(void *iface_ctx, void *dst, size_t dst_len) {
    (void)iface_ctx;
    (void)dst;
    (void)dst_len;
    return 0;
}

int lltd_port_get_wifi_max_rate_0_5mbps(void *iface_ctx, uint16_t *out_units_0_5mbps) {
    (void)iface_ctx;
    if (out_units_0_5mbps) {
        *out_units_0_5mbps = 0;
    }
    return -1;
}

int lltd_port_get_wifi_rssi_dbm(void *iface_ctx, int8_t *out_rssi_dbm) {
    (void)iface_ctx;
    if (out_rssi_dbm) {
        *out_rssi_dbm = 0;
    }
    return -1;
}

int lltd_port_get_wifi_phy_medium(void *iface_ctx, uint32_t *out_phy_medium) {
    (void)iface_ctx;
    if (out_phy_medium) {
        *out_phy_medium = 0;
    }
    return -1;
}
