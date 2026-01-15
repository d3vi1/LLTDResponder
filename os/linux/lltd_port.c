#include "../../lltdResponder/lltdPort.h"

#include "daemon/linux-main.h"

#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
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

    static const char name[] = "LLTD Responder";
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
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec;
}

uint64_t lltd_port_monotonic_milliseconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
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
    if (!iface_ctx || !out_mac) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    memcpy(out_mac->a, iface->macAddress, sizeof(out_mac->a));
    return 0;
}

uint32_t lltd_port_get_characteristics_flags(void *iface_ctx) {
    if (!iface_ctx) {
        return 0;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;

#ifndef IFM_FDX
#define IFM_FDX 0x0010
#endif

    uint32_t flags = 0;
    if (iface->MediumType & IFM_FDX) {
        flags |= Config_TLV_NetworkInterfaceDuplex_Value;
    }
    if (iface->flags & IFF_LOOPBACK) {
        flags |= Config_TLV_InterfaceIsLoopback_Value;
    }
    return flags;
}

int lltd_port_get_if_type(void *iface_ctx, uint32_t *out_if_type) {
    if (!iface_ctx || !out_if_type) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    *out_if_type = iface->ifType;
    return 0;
}

int lltd_port_get_ipv4_address(void *iface_ctx, uint32_t *out_ipv4_be) {
    if (!iface_ctx || !out_ipv4_be) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) != 0) {
        return -1;
    }

    int rc = -1;
    for (struct ifaddrs *cur = ifaddr; cur != NULL; cur = cur->ifa_next) {
        if (!cur->ifa_addr || cur->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (strcmp(iface->deviceName, cur->ifa_name) != 0) {
            continue;
        }
        *out_ipv4_be = ((struct sockaddr_in *)cur->ifa_addr)->sin_addr.s_addr;
        rc = 0;
        break;
    }

    freeifaddrs(ifaddr);
    return rc;
}

int lltd_port_get_ipv6_address(void *iface_ctx, uint8_t out_ipv6[16]) {
    if (!iface_ctx || !out_ipv6) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) != 0) {
        return -1;
    }

    int rc = -1;
    for (struct ifaddrs *cur = ifaddr; cur != NULL; cur = cur->ifa_next) {
        if (!cur->ifa_addr || cur->ifa_addr->sa_family != AF_INET6) {
            continue;
        }
        if (strcmp(iface->deviceName, cur->ifa_name) != 0) {
            continue;
        }
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)cur->ifa_addr;
        memcpy(out_ipv6, &sin6->sin6_addr, 16);
        rc = 0;
        break;
    }

    freeifaddrs(ifaddr);
    return rc;
}

int lltd_port_get_link_speed_100bps(void *iface_ctx, uint32_t *out_speed_100bps) {
    if (!iface_ctx || !out_speed_100bps) {
        return -1;
    }
    const network_interface_t *iface = (const network_interface_t *)iface_ctx;
    *out_speed_100bps = iface->LinkSpeed / 100U;
    return 0;
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
