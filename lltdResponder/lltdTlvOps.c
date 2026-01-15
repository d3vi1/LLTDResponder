/******************************************************************************
 *                                                                            *
 *   lltdTlvOps.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 26.04.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#include "lltdTlvOps.h"

#include "lltdEndian.h"
#include "lltdPort.h"

#pragma mark -
#pragma mark Host specific TLVs
size_t setHostnameTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *hostnameTLV = (generic_tlv_t *)(base + offset);
    hostnameTLV->TLVType = tlv_hostname;

    size_t written = lltd_port_get_hostname(base + offset + sizeof(*hostnameTLV), 32);
    if (written > 32) {
        written = 32;
    }
    hostnameTLV->TLVLength = (uint8_t)written;

    return sizeof(*hostnameTLV) + written;
}

size_t setHostIdTLV(void *buffer, size_t offset, void *iface_ctx) {
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *host_id = (generic_tlv_t *)(base + offset);

    host_id->TLVType = tlv_hostId;
    host_id->TLVLength = sizeof(ethernet_address_t);

    ethernet_address_t mac = {{0, 0, 0, 0, 0, 0}};
    (void)lltd_port_get_mac_address(iface_ctx, &mac);
    lltd_port_memcpy(base + offset + sizeof(*host_id), &mac, sizeof(mac));

    return sizeof(*host_id) + sizeof(mac);
}

size_t setCharacteristicsTLV(void *buffer, size_t offset, void *iface_ctx) {
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *characteristicsTLV = (generic_tlv_t *)(base + offset);

    characteristicsTLV->TLVType = tlv_characterisics;
    characteristicsTLV->TLVLength = sizeof(uint32_t);

    uint32_t flags = lltd_port_get_characteristics_flags(iface_ctx);
    uint32_t wire = lltd_htonl(flags << 16);
    lltd_port_memcpy(base + offset + sizeof(*characteristicsTLV), &wire, sizeof(wire));

    return sizeof(*characteristicsTLV) + sizeof(uint32_t);
}

size_t setPerfCounterTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *perf = (generic_tlv_t *)(base + offset);
    perf->TLVType       = tlv_perfCounterFrequency;
    perf->TLVLength     = sizeof(uint64_t);

    // Write 64-bit value in big-endian (network) order
    uint64_t freq = 1000000;  // 1 MHz performance counter frequency
    uint8_t bytes[sizeof(uint64_t)];
    bytes[0] = (freq >> 56) & 0xFF;
    bytes[1] = (freq >> 48) & 0xFF;
    bytes[2] = (freq >> 40) & 0xFF;
    bytes[3] = (freq >> 32) & 0xFF;
    bytes[4] = (freq >> 24) & 0xFF;
    bytes[5] = (freq >> 16) & 0xFF;
    bytes[6] = (freq >> 8) & 0xFF;
    bytes[7] = freq & 0xFF;
    lltd_port_memcpy(base + offset + sizeof(*perf), bytes, sizeof(bytes));
    return sizeof(generic_tlv_t) + sizeof(uint64_t);
}

size_t setIconImageTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *icon = (generic_tlv_t *)(base + offset);
    icon->TLVType       = tlv_iconImage;
    icon->TLVLength     = 0x00;
    return sizeof(*icon);
}

size_t setSupportInfoTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *supportInfoTLV = (generic_tlv_t *)(base + offset);
    supportInfoTLV->TLVType = tlv_supportUrl;

    size_t written = lltd_port_get_support_url(base + offset + sizeof(*supportInfoTLV), 64);
    if (written > 64) {
        written = 64;
    }
    supportInfoTLV->TLVLength = (uint8_t)written;

    return sizeof(*supportInfoTLV) + written;
}

size_t setFriendlyNameTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *friendly = (generic_tlv_t *)(base + offset);
    friendly->TLVType       = tlv_friendlyName;
    friendly->TLVLength     = 0x00;
    return sizeof(*friendly);
}

size_t setUuidTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *upnpUuidTLV = (generic_tlv_t *)(base + offset);
    upnpUuidTLV->TLVType = tlv_uuid;

    uint8_t uuid[16];
    if (lltd_port_get_upnp_uuid(uuid) == 0) {
        upnpUuidTLV->TLVLength = 16;
        lltd_port_memcpy(base + offset + sizeof(*upnpUuidTLV), uuid, sizeof(uuid));
        return sizeof(*upnpUuidTLV) + sizeof(uuid);
    }

    upnpUuidTLV->TLVLength = 0;
    return sizeof(*upnpUuidTLV);
}

// Hardware ID TLV - sends the hardware identifier (platform UUID as UCS-2LE)
// Returns size of TLV written, or 0 if no hardware ID available
size_t setHardwareIdTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *hwIdTLV = (generic_tlv_t *)(base + offset);

    uint8_t *payload = base + offset + sizeof(*hwIdTLV);
    size_t dataLen = lltd_port_get_hw_id(payload, 64);
    if (dataLen > 64) {
        dataLen = 64;
    }
    if (dataLen == 0) {
        return 0;
    }

    hwIdTLV->TLVType = tlv_hwIdProperty;
    hwIdTLV->TLVLength = (uint8_t)dataLen;
    return sizeof(*hwIdTLV) + dataLen;
}

//TODO: see if there really is support for Level2 Forwarding.. ? or just leave it hardcoded
size_t setQosCharacteristicsTLV(void *buffer, size_t offset){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *QosCharacteristicsTLV = (generic_tlv_t *)(base + offset);
    QosCharacteristicsTLV->TLVType       = tlv_qos_characteristics;
    QosCharacteristicsTLV->TLVLength     = sizeof(uint32_t);
    // QoS flags are in upper 16 bits of 32-bit value
    uint32_t qosCharacteristics = lltd_htonl((Config_TLV_QOS_L2Fwd | Config_TLV_QOS_PrioTag | Config_TLV_QOS_VLAN) << 16);
    lltd_port_memcpy(base + offset + sizeof(*QosCharacteristicsTLV), &qosCharacteristics, sizeof(qosCharacteristics));
    return sizeof(generic_tlv_t) + sizeof(uint32_t);
}

// Detailed Icon TLV - sends the detailed icon image (multi-resolution ICO)
// Only used with QueryLargeTLV. Returns raw icon data without TLV header.
size_t setDetailedIconTLV(void *buffer, size_t offset){
    (void)buffer;
    (void)offset;
    return 0;
}

// Component Table TLV - sends the component descriptor table
// Only used with QueryLargeTLV. Returns raw component data without TLV header.
size_t setComponentTableTLV(void *buffer, size_t offset){
    (void)buffer;
    (void)offset;
    return 0;
}

size_t setEndOfPropertyTLV(void *buffer, size_t offset){
    uint8_t *eop  = (uint8_t *)buffer + offset;
    *eop = eofpropmarker;
    return sizeof (*eop);
}

#pragma mark -
#pragma mark Interface Specific TLVs
size_t setPhysicalMediumTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *hdr = (generic_tlv_t *)(base + offset);

    hdr->TLVType = tlv_ifType;
    hdr->TLVLength = sizeof(uint32_t);

    uint32_t ifType = 0;
    (void)lltd_port_get_if_type(iface_ctx, &ifType);
    uint32_t wire = lltd_htonl(ifType);
    lltd_port_memcpy(base + offset + sizeof(*hdr), &wire, sizeof(wire));

    return sizeof(*hdr) + sizeof(wire);
}

size_t setIPv4TLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *hdr = (generic_tlv_t *)(base + offset);

    hdr->TLVType = tlv_ipv4;
    hdr->TLVLength = sizeof(uint32_t);

    uint32_t ipv4_be = 0;
    (void)lltd_port_get_ipv4_address(iface_ctx, &ipv4_be);
    lltd_port_memcpy(base + offset + sizeof(*hdr), &ipv4_be, sizeof(ipv4_be));

    return sizeof(*hdr) + sizeof(ipv4_be);
}

size_t setIPv6TLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *hdr = (generic_tlv_t *)(base + offset);

    hdr->TLVType = tlv_ipv6;
    hdr->TLVLength = 16;

    uint8_t ipv6[16];
    lltd_port_memset(ipv6, 0, sizeof(ipv6));
    (void)lltd_port_get_ipv6_address(iface_ctx, ipv6);
    lltd_port_memcpy(base + offset + sizeof(*hdr), ipv6, sizeof(ipv6));

    return sizeof(*hdr) + sizeof(ipv6);
}

size_t setLinkSpeedTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *linkSpeedTL = (generic_tlv_t *)(base + offset);
    linkSpeedTL->TLVType = tlv_linkSpeed;
    linkSpeedTL->TLVLength = sizeof(uint32_t);

    uint32_t speed_100bps = 0;
    (void)lltd_port_get_link_speed_100bps(iface_ctx, &speed_100bps);
    uint32_t wire = lltd_htonl(speed_100bps);
    lltd_port_memcpy(base + offset + sizeof(*linkSpeedTL), &wire, sizeof(wire));

    return sizeof(*linkSpeedTL) + sizeof(wire);
}

size_t setSeeslistWorkingSetTLV(void *buffer, size_t offset){
    (void)buffer;
    (void)offset;
    return 0;
}

#pragma mark -
#pragma mark 802.11 Interface Specific TLVs
size_t setWirelessTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t mode = 0;
    if (lltd_port_get_wifi_mode(iface_ctx, &mode) != 0) {
        return 0;
    }

    uint8_t *base = (uint8_t *)buffer;
    wireless_tlv_t *wifiTlv = (wireless_tlv_t *)(base + offset);
    wifiTlv->TLVType = tlv_wifimode;
    wifiTlv->TLVLength = 1;
    wifiTlv->WIFIMode = mode;
    return sizeof(*wifiTlv);
}

size_t setComponentTable(void *buffer, size_t offset){
    return 0;
}

size_t setBSSIDTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t bssid[6];
    if (lltd_port_get_bssid(iface_ctx, bssid) != 0) {
        return 0;
    }

    uint8_t *base = (uint8_t *)buffer;
    bssid_tlv_t *bssidTlv = (bssid_tlv_t *)(base + offset);
    bssidTlv->TLVType = tlv_bssid;
    bssidTlv->TLVLength = 6;
    lltd_port_memcpy(bssidTlv->macAddress, bssid, sizeof(bssid));
    return sizeof(*bssidTlv);
}

size_t setSSIDTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *ssidTlv = (generic_tlv_t *)(base + offset);
    ssidTlv->TLVType = tlv_ssid;
    ssidTlv->TLVLength = 0;

    size_t ssidSize = lltd_port_get_ssid(iface_ctx, base + offset + sizeof(*ssidTlv), 32);
    if (ssidSize > 32) {
        ssidSize = 32;
    }

    ssidTlv->TLVLength = (uint8_t)ssidSize;
    return sizeof(*ssidTlv) + ssidSize;
}

size_t setWifiMaxRateTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *rateTlv = (generic_tlv_t *)(base + offset);
    rateTlv->TLVType = tlv_wifiMaxRate;
    rateTlv->TLVLength = sizeof(uint16_t);

    uint16_t units_0_5mbps = 0;
    (void)lltd_port_get_wifi_max_rate_0_5mbps(iface_ctx, &units_0_5mbps);
    uint16_t wire = lltd_htons(units_0_5mbps);
    lltd_port_memcpy(base + offset + sizeof(*rateTlv), &wire, sizeof(wire));

    return sizeof(*rateTlv) + sizeof(wire);
}

size_t setWifiRssiTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *rssiTlv = (generic_tlv_t *)(base + offset);
    rssiTlv->TLVType = tlv_wifiRssi;
    rssiTlv->TLVLength = sizeof(int32_t);

    int8_t rssi_dbm = 0;
    (void)lltd_port_get_wifi_rssi_dbm(iface_ctx, &rssi_dbm);
    uint32_t wire = lltd_htonl((uint32_t)(int32_t)rssi_dbm);
    lltd_port_memcpy(base + offset + sizeof(*rssiTlv), &wire, sizeof(wire));
    return sizeof(*rssiTlv) + sizeof(wire);
}

size_t set80211MediumTLV(void *buffer, size_t offset, void *iface_ctx){
    uint8_t *base = (uint8_t *)buffer;
    generic_tlv_t *mediumTlv = (generic_tlv_t *)(base + offset);
    mediumTlv->TLVType = tlv_ifType;
    mediumTlv->TLVLength = sizeof(uint32_t);

    uint32_t phyMedium = 0;
    (void)lltd_port_get_wifi_phy_medium(iface_ctx, &phyMedium);
    uint32_t wire = lltd_htonl(phyMedium);
    lltd_port_memcpy(base + offset + sizeof(*mediumTlv), &wire, sizeof(wire));
    return sizeof(*mediumTlv) + sizeof(wire);
}

size_t setAPAssociationTableTLV(void *buffer, size_t offset, void *networkInterface){
    return 0;
}

size_t setRepeaterAPLineageTLV(void *buffer, size_t offset, void *networkInterface){
    return 0;
}

size_t setRepeaterAPTableTLV(void *buffer, size_t offset, void *networkInterface){
    return 0;
}
