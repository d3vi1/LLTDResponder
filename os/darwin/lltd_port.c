#include "../../lltdResponder/lltdPort.h"

#include "../daemon/lltdDaemon.h"

#include <mach/mach_time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

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
    if (!iface_ctx || !frame || frame_len == 0) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    if (iface->socket < 0) {
        return -1;
    }
    ssize_t written = sendto(iface->socket,
                             frame,
                             frame_len,
                             0,
                             (const struct sockaddr *)&iface->socketAddr,
                             sizeof(iface->socketAddr));
    return (written == (ssize_t)frame_len) ? 0 : -1;
}

int lltd_port_get_mtu(void *iface_ctx, size_t *out_mtu) {
    if (!iface_ctx || !out_mtu) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    *out_mtu = (size_t)iface->MTU;
    return 0;
}

int lltd_port_get_icon_image(void **out_data, size_t *out_size) {
    if (!out_data || !out_size) {
        return -1;
    }
    *out_data = NULL;
    *out_size = 0;
    getIconImage(out_data, out_size);
    return (*out_data && *out_size) ? 0 : -1;
}

int lltd_port_get_friendly_name(void **out_data, size_t *out_size) {
    if (!out_data || !out_size) {
        return -1;
    }
    *out_data = NULL;
    *out_size = 0;
    getFriendlyName((char **)out_data, out_size);
    return (*out_data && *out_size) ? 0 : -1;
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

size_t lltd_port_get_hostname(void *dst, size_t dst_len) {
    if (!dst || dst_len == 0) {
        return 0;
    }

    char *hostname = NULL;
    size_t hostname_len = 0;
    getMachineName(&hostname, &hostname_len);

    if (!hostname || hostname_len == 0) {
        free(hostname);
        return 0;
    }

    if (hostname_len > dst_len) {
        hostname_len = dst_len;
    }
    memcpy(dst, hostname, hostname_len);
    free(hostname);
    return hostname_len;
}

size_t lltd_port_get_support_url(void *dst, size_t dst_len) {
    if (!dst || dst_len == 0) {
        return 0;
    }

    void *url = NULL;
    size_t url_len = 0;
    getSupportInfo(&url, &url_len);

    if (!url || url_len == 0) {
        free(url);
        return 0;
    }

    if (url_len > dst_len) {
        url_len = dst_len;
    }
    memcpy(dst, url, url_len);
    free(url);
    return url_len;
}

int lltd_port_get_upnp_uuid(uint8_t out_uuid[16]) {
    if (!out_uuid) {
        return -1;
    }

    void *uuid = NULL;
    getUpnpUuid(&uuid);
    if (!uuid) {
        return -1;
    }

    memcpy(out_uuid, uuid, 16);
    free(uuid);
    return 0;
}

size_t lltd_port_get_hw_id(void *dst, size_t dst_len) {
    if (!dst || dst_len == 0) {
        return 0;
    }

    uint8_t hwid[64];
    memset(hwid, 0, sizeof(hwid));
    getHwId(hwid);

    size_t len = 0;
    for (size_t i = 0; i + 1 < sizeof(hwid); i += 2) {
        if (hwid[i] != 0 || hwid[i + 1] != 0) {
            len = i + 2;
        }
    }

    if (len > dst_len) {
        len = dst_len;
    }
    memcpy(dst, hwid, len);
    return len;
}

int lltd_port_get_mac_address(void *iface_ctx, ethernet_address_t *out_mac) {
    if (!iface_ctx || !out_mac) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    memcpy(out_mac->a, iface->macAddress, sizeof(out_mac->a));
    return 0;
}

uint32_t lltd_port_get_characteristics_flags(void *iface_ctx) {
    uint16_t flags = 0;
    if (getIfCharacteristics(&flags, iface_ctx)) {
        return flags;
    }
    return 0;
}

int lltd_port_get_if_type(void *iface_ctx, uint32_t *out_if_type) {
    if (!out_if_type) {
        return -1;
    }
    uint32_t ifType = 0;
    if (!getIfIANAType(&ifType, iface_ctx)) {
        *out_if_type = 0;
        return -1;
    }
    *out_if_type = ifType;
    return 0;
}

int lltd_port_get_ipv4_address(void *iface_ctx, uint32_t *out_ipv4_be) {
    if (!out_ipv4_be) {
        return -1;
    }
    uint32_t ipv4 = 0;
    if (!getIfIPv4info(&ipv4, iface_ctx)) {
        *out_ipv4_be = 0;
        return -1;
    }
    *out_ipv4_be = ipv4;
    return 0;
}

int lltd_port_get_ipv6_address(void *iface_ctx, uint8_t out_ipv6[16]) {
    if (!out_ipv6) {
        return -1;
    }
    struct in6_addr ipv6;
    memset(&ipv6, 0, sizeof(ipv6));
    if (!getIfIPv6info(&ipv6, iface_ctx)) {
        memset(out_ipv6, 0, 16);
        return -1;
    }
    memcpy(out_ipv6, &ipv6, 16);
    return 0;
}

int lltd_port_get_link_speed_100bps(void *iface_ctx, uint32_t *out_speed_100bps) {
    if (!iface_ctx || !out_speed_100bps) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    *out_speed_100bps = (uint32_t)(iface->LinkSpeed / 100ULL);
    return 0;
}

int lltd_port_get_wifi_mode(void *iface_ctx, uint8_t *out_mode) {
    if (!out_mode) {
        return -1;
    }
    uint8_t mode = 0;
    if (!WgetWirelessMode(&mode, iface_ctx)) {
        *out_mode = 0;
        return -1;
    }
    *out_mode = mode;
    return 0;
}

int lltd_port_get_bssid(void *iface_ctx, uint8_t out_bssid[6]) {
    if (!out_bssid) {
        return -1;
    }
    void *bssid = NULL;
    if (!WgetIfBSSID(&bssid, iface_ctx) || !bssid) {
        memset(out_bssid, 0, 6);
        free(bssid);
        return -1;
    }
    memcpy(out_bssid, bssid, 6);
    free(bssid);
    return 0;
}

size_t lltd_port_get_ssid(void *iface_ctx, void *dst, size_t dst_len) {
    if (!dst || dst_len == 0) {
        return 0;
    }

    char *ssid = NULL;
    size_t ssid_len = 0;
    if (!WgetIfSSID(&ssid, &ssid_len, iface_ctx) || !ssid || ssid_len == 0) {
        free(ssid);
        return 0;
    }

    if (ssid_len > dst_len) {
        ssid_len = dst_len;
    }
    memcpy(dst, ssid, ssid_len);
    free(ssid);
    return ssid_len;
}

int lltd_port_get_wifi_max_rate_0_5mbps(void *iface_ctx, uint16_t *out_units_0_5mbps) {
    if (!out_units_0_5mbps) {
        return -1;
    }
    uint32_t rateMbps = 0;
    if (!WgetIfMaxRate(&rateMbps, iface_ctx)) {
        *out_units_0_5mbps = 0;
        return -1;
    }
    uint32_t units = rateMbps * 2U;
    if (units > 0xFFFFu) {
        units = 0xFFFFu;
    }
    *out_units_0_5mbps = (uint16_t)units;
    return 0;
}

int lltd_port_get_wifi_rssi_dbm(void *iface_ctx, int8_t *out_rssi_dbm) {
    if (!out_rssi_dbm) {
        return -1;
    }
    if (!WgetIfRSSI(out_rssi_dbm, iface_ctx)) {
        *out_rssi_dbm = 0;
        return -1;
    }
    return 0;
}

int lltd_port_get_wifi_phy_medium(void *iface_ctx, uint32_t *out_phy_medium) {
    if (!out_phy_medium) {
        return -1;
    }
    uint32_t medium = 0;
    if (!WgetIfPhyMedium(&medium, iface_ctx)) {
        *out_phy_medium = 0;
        return -1;
    }
    *out_phy_medium = medium;
    return 0;
}
