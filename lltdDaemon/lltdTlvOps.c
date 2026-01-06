/******************************************************************************
 *                                                                            *
 *   lltdTlvOps.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 26.04.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifdef LLTD_TESTING
#include "lltdTestShim.h"
#else
#include "lltdDaemon.h"
#endif

#pragma mark -
#pragma mark Header generation

size_t setLltdHeader (void *buffer, ethernet_address_t *source, ethernet_address_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos){
    
    lltd_demultiplex_header_t *lltdHeader = (lltd_demultiplex_header_t*) buffer;
    
    lltdHeader->frameHeader.ethertype = htons(lltdEtherType);
    memcpy(&lltdHeader->frameHeader.source, source, sizeof(ethernet_address_t));
    memcpy(&lltdHeader->frameHeader.destination, destination, sizeof(ethernet_address_t));
    memcpy(&lltdHeader->realSource, source, sizeof(ethernet_address_t));
    memcpy(&lltdHeader->realDestination, destination, sizeof(ethernet_address_t));
    lltdHeader->seqNumber = seqNumber;
    lltdHeader->opcode = opcode;
    lltdHeader->tos = tos;
    lltdHeader->version = 1;
    
    return sizeof(lltd_demultiplex_header_t);
}

bool compareEthernetAddress(const ethernet_address_t *A, const ethernet_address_t *B) {
    return A->a[0]==B->a[0] &&
           A->a[1]==B->a[1] &&
           A->a[2]==B->a[2] &&
           A->a[3]==B->a[3] &&
           A->a[4]==B->a[4] &&
           A->a[5]==B->a[5];
}

//
// Get the mess out of lltdBlock.c
//
size_t setHelloHeader (void *buffer, uint64_t offset, ethernet_address_t *apparentMapper, ethernet_address_t *currentMapper, uint16_t generation){
    
    lltd_hello_upper_header_t *helloHeader = (lltd_hello_upper_header_t *) (buffer+offset);

    memcpy(&helloHeader->apparentMapper, apparentMapper, sizeof(ethernet_address_t));
    memcpy(&helloHeader->currentMapper, currentMapper, sizeof(ethernet_address_t));
    helloHeader->generation = generation;
    
    return sizeof(lltd_hello_upper_header_t);
}

#pragma mark -
#pragma mark Host specific TLVs
size_t setHostnameTLV(void *buffer, uint64_t offset){
    void *hostname = NULL;
    size_t sizeOfHostname = 0;
    getMachineName((char **)&hostname, &sizeOfHostname);
    if (sizeOfHostname > 32) {
        sizeOfHostname = 32;
    }
    generic_tlv_t *hostnameTLV = (generic_tlv_t *) (buffer + offset);
    hostnameTLV->TLVType = tlv_hostname;
    hostnameTLV->TLVLength = sizeOfHostname;
    memcpy((void *)(buffer + offset + sizeof(generic_tlv_t)), hostname, sizeOfHostname);
    free(hostname);
    return (sizeof(generic_tlv_t) + sizeOfHostname);
}

size_t setHostIdTLV(void *buffer, uint64_t offset, void *networkInterface) {
    generic_tlv_t * host_id = (generic_tlv_t *) (buffer + offset);
    
    network_interface_t *currentNetworkInterface = networkInterface;
    host_id->TLVType = tlv_hostId;
    host_id->TLVLength = sizeof(ethernet_address_t);
    memcpy((void *)(buffer+offset+sizeof(*host_id)), &(currentNetworkInterface->macAddress), sizeof(ethernet_address_t));
    return (sizeof(*host_id) + sizeof(currentNetworkInterface->macAddress));
}

size_t setCharacteristicsTLV(void *buffer, uint64_t offset, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *characteristicsTLV = (generic_tlv_t *) (buffer+offset);
    uint16_t *characteristicsValue = (void *) (buffer + offset + sizeof(*characteristicsTLV));
    characteristicsTLV->TLVType = tlv_characterisics;
    characteristicsTLV->TLVLength = sizeof(*characteristicsValue);
    
    //Checking if we're full duplex
    if (currentNetworkInterface->MediumType & IFM_FDX){
        *characteristicsValue = htons(Config_TLV_NetworkInterfaceDuplex_Value);
    }
    //shouldn't be here EVER because we don't start on loopback interfaces
    if (currentNetworkInterface->flags & IFF_LOOPBACK){
        *characteristicsValue = htons(Config_TLV_InterfaceIsLoopback_Value);
    }
    return (sizeof(*characteristicsTLV) + sizeof(*characteristicsValue));
}

size_t setPerfCounterTLV(void *buffer, uint64_t offset){
    generic_tlv_t *perf = (generic_tlv_t *) (buffer+offset);
    uint32_t *perfValue = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));
    perf->TLVType       = tlv_perfCounterFrequency;
    perf->TLVLength     = sizeof(*perfValue);
   *perfValue           = htonl(1000000);
    return sizeof(generic_tlv_t)+sizeof(*perfValue);
}

size_t setIconImageTLV(void *buffer, uint64_t offset){
    generic_tlv_t *icon = (generic_tlv_t *) (buffer+offset);
    icon->TLVType       = tlv_iconImage;
    icon->TLVLength     = 0x00;
    return sizeof(*icon);
}

size_t setSupportInfoTLV(void *buffer, uint64_t offset){
    void  *supportInfoURL = NULL;
    size_t sizeOfURL      = 0;
    getSupportInfo(&supportInfoURL, &sizeOfURL);

    if (sizeOfURL > 64) {
        sizeOfURL = 64;
    }
    
    generic_tlv_t *supportInfoTLV = (generic_tlv_t *) (buffer + offset);
    supportInfoTLV->TLVType = tlv_supportUrl;
    supportInfoTLV->TLVLength = sizeOfURL;
    memcpy((void *)(buffer + offset + sizeof(generic_tlv_t)), supportInfoURL, sizeOfURL);
    free(supportInfoURL);
    return (sizeof(generic_tlv_t) + sizeOfURL);
}

size_t setFriendlyNameTLV(void *buffer, uint64_t offset){
    generic_tlv_t *friendly = (generic_tlv_t *) (buffer+offset);
    friendly->TLVType       = tlv_friendlyName;
    friendly->TLVLength     = 0x00;
    return sizeof(*friendly);
}

size_t setUuidTLV(void *buffer, uint64_t offset){
    void *machineUUID = NULL;
    getUpnpUuid(&machineUUID);

    generic_tlv_t *upnpUuidTLV = (generic_tlv_t *) (buffer + offset);
    upnpUuidTLV->TLVType = tlv_uuid;

    // UUID is always 16 bytes
    if (machineUUID != NULL) {
        upnpUuidTLV->TLVLength = 16;
        // Copy the UUID bytes from machineUUID, not from the TLV header
        memcpy((void *)(buffer + offset + sizeof(generic_tlv_t)), machineUUID, 16);
        free(machineUUID);
        return (sizeof(generic_tlv_t) + 16);
    } else {
        // No UUID available - emit TLV with zero length
        upnpUuidTLV->TLVLength = 0;
        return sizeof(generic_tlv_t);
    }
}

// Hardware ID TLV - sends the hardware identifier (platform UUID as UCS-2LE)
// Returns size of TLV written, or 0 if no hardware ID available
size_t setHardwareIdTLV(void *buffer, uint64_t offset){
    // Get the platform UUID as hardware ID (platform-specific implementation)
    uint8_t hwIdData[64];
    memset(hwIdData, 0, sizeof(hwIdData));
    getHwId(hwIdData);

    // Check if we got any data (find last non-zero UCS-2LE char)
    size_t dataLen = 0;
    for (size_t i = 0; i < 64; i += 2) {
        if (hwIdData[i] != 0 || hwIdData[i+1] != 0) {
            dataLen = i + 2;
        }
    }

    if (dataLen > 0) {
        generic_tlv_t *hwIdTLV = (generic_tlv_t *)(buffer + offset);
        hwIdTLV->TLVType = tlv_hwIdProperty;
        hwIdTLV->TLVLength = (uint8_t)dataLen;
        memcpy((void *)(buffer + offset + sizeof(generic_tlv_t)), hwIdData, dataLen);
        return sizeof(generic_tlv_t) + dataLen;
    }

    // No hardware ID available - don't emit the TLV at all
    return 0;
}

//TODO: see if there really is support for Level2 Forwarding.. ? or just leave it hardcoded
size_t setQosCharacteristicsTLV(void *buffer, uint64_t offset){
    generic_tlv_t *QosCharacteristicsTLV = (generic_tlv_t *) (buffer + offset);
    uint32_t *qosCharacteristics         = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));
    QosCharacteristicsTLV->TLVType       = tlv_qos_characteristics;
    QosCharacteristicsTLV->TLVLength     = sizeof(*qosCharacteristics);
    *qosCharacteristics                  = htonl(Config_TLV_QOS_L2Fwd | Config_TLV_QOS_PrioTag | Config_TLV_QOS_VLAN);
    return sizeof(generic_tlv_t) + sizeof(uint32_t);
}

// Detailed Icon TLV - sends the detailed icon image (multi-resolution ICO)
// Only used with QueryLargeTLV. Returns raw icon data without TLV header.
size_t setDetailedIconTLV(void *buffer, uint64_t offset){
    void *iconData = NULL;
    size_t iconSize = 0;
    getDetailedIconImage(&iconData, &iconSize);

    if (iconData != NULL && iconSize > 0) {
        // For QueryLargeTLV response, we write raw data without TLV header
        memcpy((void *)(buffer + offset), iconData, iconSize);
        free(iconData);
        return iconSize;
    }
    if (iconData) {
        free(iconData);
    }
    return 0;
}

// Component Table TLV - sends the component descriptor table
// Only used with QueryLargeTLV. Returns raw component data without TLV header.
size_t setComponentTableTLV(void *buffer, uint64_t offset){
    void *tableData = NULL;
    size_t tableSize = 0;
    getComponentTable(&tableData, &tableSize);

    if (tableData != NULL && tableSize > 0) {
        // For QueryLargeTLV response, we write raw data without TLV header
        memcpy((void *)(buffer + offset), tableData, tableSize);
        free(tableData);
        return tableSize;
    }
    if (tableData) {
        free(tableData);
    }
    return 0;
}

size_t setEndOfPropertyTLV(void *buffer, uint64_t offset){
    uint8_t *eop  = (uint8_t *) (buffer + offset);
    *eop = eofpropmarker;
    return sizeof (*eop);
}

#pragma mark -
#pragma mark Interface Specific TLVs
size_t setPhysicalMediumTLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *hdr = (generic_tlv_t *) (buffer + offset);

    hdr->TLVType = tlv_ifType;
    hdr->TLVLength = sizeof(uint32_t);

    uint32_t *ifType = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));
    *ifType          = htonl((uint32_t)currentNetworkInterface->ifType);
    return sizeof(generic_tlv_t) + sizeof(uint32_t);
}

size_t setIPv4TLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *hdr = (generic_tlv_t *) (buffer + offset);
    hdr->TLVType = tlv_ipv4;
    hdr->TLVLength = sizeof(uint32_t);
    
    uint32_t *ipv4 = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));

    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int retVal = 0;
    retVal = getifaddrs(&interfaces);
    
    boolean_t foundIPv4 = false;
    
    if (retVal == KERN_SUCCESS) {
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if (! strcmp(currentNetworkInterface->deviceName, temp_addr->ifa_name)) {
                
                if(temp_addr->ifa_addr->sa_family == AF_INET && !foundIPv4) {
                    *ipv4 = ((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr.s_addr;
                    foundIPv4 = true;
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);

    return sizeof (*hdr) + sizeof(uint32_t);
}

size_t setIPv6TLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *hdr = (generic_tlv_t *) (buffer + offset);
    
    struct in6_addr *ipv6 = (buffer + offset + sizeof(*hdr));
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int retVal = 0;
    retVal = getifaddrs(&interfaces);
    boolean_t foundIPv6 = false;
    
    if (retVal == KERN_SUCCESS) {
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if (! strcmp(currentNetworkInterface->deviceName, temp_addr->ifa_name)) {
                
                if(temp_addr->ifa_addr->sa_family == AF_INET6 && !foundIPv6) {
                    *ipv6 = ((struct sockaddr_in6 *)temp_addr->ifa_addr)->sin6_addr;
                    foundIPv6 = true;
                }
            }
            
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);

    hdr->TLVType = tlv_ipv6;
    hdr->TLVLength = sizeof(*ipv6);
    return sizeof (*hdr) + hdr->TLVLength;
}

size_t setLinkSpeedTLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    uint32_t *linkSpeedValue = (buffer + offset + sizeof(generic_tlv_t));
    generic_tlv_t *linkSpeedTL = (generic_tlv_t *) (buffer + offset);
    linkSpeedTL->TLVType   = tlv_linkSpeed;
    linkSpeedTL->TLVLength = sizeof(*linkSpeedValue);
    
    *linkSpeedValue = htonl((uint32_t)(currentNetworkInterface->LinkSpeed)/100);
    
    return (sizeof(*linkSpeedTL) + sizeof(*linkSpeedValue));
}

size_t setSeeslistWorkingSetTLV(void *buffer, uint64_t offset){
    
}

#pragma mark -
#pragma mark 802.11 Interface Specific TLVs
size_t setWirelessTLV(void *buffer, uint64_t offset, void *networkInterface){
    wireless_tlv_t *wifiTlv = (wireless_tlv_t *) (buffer + offset);
    wifiTlv->TLVType        = tlv_wifimode;
    wifiTlv->TLVLength      = 1;
    wifiTlv->WIFIMode       = 0x0;
    wifiTlv->WIFIMode       = getWifiMode(networkInterface);
    return sizeof(wireless_tlv_t);
}

size_t setComponentTable(void *buffer, uint64_t offset){
    return 0;
}

size_t setBSSIDTLV(void *buffer, uint64_t offset, void *networkInterface){
    bssid_tlv_t *bssidTlv = (bssid_tlv_t *) (buffer + offset);
    bssidTlv->TLVType        = tlv_bssid;
    bssidTlv->TLVLength      = 6;

    void *bssidData = NULL;
    if (getBSSID(&bssidData, networkInterface) && bssidData) {
        memcpy(bssidTlv->macAddress, bssidData, 6);
        free(bssidData);
        return sizeof(bssid_tlv_t);
    }
    return 0;
}

size_t setSSIDTLV(void *buffer, uint64_t offset, void *networkInterface){
    generic_tlv_t *ssidTlv = (generic_tlv_t *) (buffer + offset);
    ssidTlv->TLVType = tlv_ssid;
    ssidTlv->TLVLength = 0;

    char *ssid = NULL;
    size_t ssidSize = 0;
    if (getSSID(&ssid, &ssidSize, networkInterface) && ssid && ssidSize > 0) {
        if (ssidSize > 32) {
            ssidSize = 32;
        }
        ssidTlv->TLVLength = (uint8_t)ssidSize;
        memcpy((void *)(buffer + offset + sizeof(generic_tlv_t)), ssid, ssidSize);
        free(ssid);
        return sizeof(generic_tlv_t) + ssidSize;
    }
    if (ssid) {
        free(ssid);
    }
    return sizeof(generic_tlv_t);
}

size_t setWifiMaxRateTLV(void *buffer, uint64_t offset, void *networkInterface){
    generic_tlv_t *rateTlv = (generic_tlv_t *) (buffer + offset);
    rateTlv->TLVType = tlv_wifiMaxRate;
    rateTlv->TLVLength = sizeof(uint16_t);

    uint16_t *rate = (uint16_t *)(buffer + offset + sizeof(generic_tlv_t));
    *rate = 0;

    // Get rate in Mbps and convert to 0.5 Mbps units for LLTD spec
    uint32_t rateMbps = 0;
    if (getWifiMaxRate(&rateMbps, networkInterface)) {
        *rate = htons((uint16_t)(rateMbps * 2));  // Convert Mbps to 0.5 Mbps units
    }
    return sizeof(generic_tlv_t) + sizeof(uint16_t);
}

size_t setWifiRssiTLV(void *buffer, uint64_t offset, void *networkInterface){
    generic_tlv_t *rssiTlv = (generic_tlv_t *) (buffer + offset);
    rssiTlv->TLVType = tlv_wifiRssi;
    rssiTlv->TLVLength = sizeof(int32_t);

    int32_t *rssi = (int32_t *)(buffer + offset + sizeof(generic_tlv_t));
    *rssi = 0;

    int8_t rssiValue = 0;
    if (getWifiRssi(&rssiValue, networkInterface)) {
        *rssi = htonl((int32_t)rssiValue);
    }
    return sizeof(generic_tlv_t) + sizeof(int32_t);
}

size_t set80211MediumTLV(void *buffer, uint64_t offset, void *networkInterface){
    generic_tlv_t *mediumTlv = (generic_tlv_t *) (buffer + offset);
    mediumTlv->TLVType = tlv_ifType;
    mediumTlv->TLVLength = sizeof(uint32_t);

    uint32_t *medium = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));
    *medium = 0;

    uint32_t phyMedium = 0;
    if (getWifiPhyMedium(&phyMedium, networkInterface)) {
        *medium = htonl(phyMedium);
    }
    return sizeof(generic_tlv_t) + sizeof(uint32_t);
}

size_t setAPAssociationTableTLV(void *buffer, uint64_t offset, void *networkInterface){
    return 0;
}

size_t setRepeaterAPLineageTLV(void *buffer, uint64_t offset, void *networkInterface){
    return 0;
}

size_t setRepeaterAPTableTLV(void *buffer, uint64_t offset, void *networkInterface){
    return 0;
}
