#ifndef LLTD_RESPONDER_PORT_API_H
#define LLTD_RESPONDER_PORT_API_H

#include <stddef.h>
#include <stdint.h>

#include "lltdProtocol.h"

uint64_t lltd_port_monotonic_seconds(void);
uint64_t lltd_port_monotonic_milliseconds(void);

void *lltd_port_malloc(size_t size);
void lltd_port_free(void *ptr);
void *lltd_port_memset(void *ptr, int value, size_t num);
void *lltd_port_memcpy(void *destination, const void *source, size_t num);
int lltd_port_memcmp(const void *lhs, const void *rhs, size_t num);

void lltd_port_sleep_ms(uint32_t milliseconds);

int lltd_port_send_frame(void *iface_ctx, const void *frame, size_t frame_len);

int lltd_port_get_mtu(void *iface_ctx, size_t *out_mtu);
int lltd_port_get_icon_image(void **out_data, size_t *out_size);
int lltd_port_get_friendly_name(void **out_data, size_t *out_size);

size_t lltd_port_get_hostname(void *dst, size_t dst_len);
size_t lltd_port_get_support_url(void *dst, size_t dst_len);
int lltd_port_get_upnp_uuid(uint8_t out_uuid[16]);
size_t lltd_port_get_hw_id(void *dst, size_t dst_len);

int lltd_port_get_mac_address(void *iface_ctx, ethernet_address_t *out_mac);
uint32_t lltd_port_get_characteristics_flags(void *iface_ctx);
int lltd_port_get_if_type(void *iface_ctx, uint32_t *out_if_type);
int lltd_port_get_ipv4_address(void *iface_ctx, uint32_t *out_ipv4_be);
int lltd_port_get_ipv6_address(void *iface_ctx, uint8_t out_ipv6[16]);
int lltd_port_get_link_speed_100bps(void *iface_ctx, uint32_t *out_speed_100bps);

int lltd_port_get_wifi_mode(void *iface_ctx, uint8_t *out_mode);
int lltd_port_get_bssid(void *iface_ctx, uint8_t out_bssid[6]);
size_t lltd_port_get_ssid(void *iface_ctx, void *dst, size_t dst_len);
int lltd_port_get_wifi_max_rate_0_5mbps(void *iface_ctx, uint16_t *out_units_0_5mbps);
int lltd_port_get_wifi_rssi_dbm(void *iface_ctx, int8_t *out_rssi_dbm);
int lltd_port_get_wifi_phy_medium(void *iface_ctx, uint32_t *out_phy_medium);

void lltd_port_log_debug(const char *fmt, ...);
void lltd_port_log_warning(const char *fmt, ...);

#endif
