/******************************************************************************
 *                                                                            *
 *   lltdTlvOps.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 26.04.2014.                      *
 *   Copyright © 2014 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#include    "lltdDaemon.h"

#pragma mark -
#pragma mark Header generation

uint64_t setLltdHeader (void *buffer, ethernet_address_t *source, ethernet_address_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos){
    
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
uint64_t setHelloHeader (void *buffer, uint64_t offset, ethernet_address_t *apparentMapper, ethernet_address_t *currentMapper, uint16_t generation){
    
    lltd_hello_upper_header_t *helloHeader = (lltd_hello_upper_header_t *) (buffer+offset);

    memcpy(&helloHeader->apparentMapper, apparentMapper, sizeof(ethernet_address_t));
    memcpy(&helloHeader->currentMapper, currentMapper, sizeof(ethernet_address_t));
    helloHeader->generation = generation;
    
    return sizeof(lltd_hello_upper_header_t);
}

#pragma mark -
#pragma mark Host specific TLVs
uint64_t setHostnameTLV(void *buffer, uint64_t offset){
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

uint64_t setHostIdTLV(void *buffer, uint64_t offset, void *networkInterface) {
    generic_tlv_t * host_id = (generic_tlv_t *) (buffer + offset);
    
    network_interface_t *currentNetworkInterface = networkInterface;
    host_id->TLVType = tlv_hostId;
    host_id->TLVLength = sizeof(ethernet_address_t);
    memcpy((void *)(buffer+offset+sizeof(*host_id)), &(currentNetworkInterface->macAddress), sizeof(ethernet_address_t));
    return (sizeof(*host_id) + sizeof(currentNetworkInterface->macAddress));
}

uint64_t setCharacteristicsTLV(void *buffer, uint64_t offset, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *characteristicsTLV = (generic_tlv_t *) (buffer+offset);
    uint16_t *characteristicsValue = (void *) (buffer + offset + sizeof(*characteristicsTLV));
    characteristicsTLV->TLVType = tlv_characterisics;
    characteristicsTLV->TLVLength = sizeof(*characteristicsValue);
    
    //Checking if we're full duplex
    if (currentNetworkInterface->MediumType & IFM_FDX){
        *characteristicsValue = htons(Config_TLV_NetworkInterfaceDuplex_Value);
    }
    uint32_t flags;
    CFNumberGetValue(currentNetworkInterface->flags, kCFNumberIntType, &flags);
    if (flags & IFF_LOOPBACK){
        *characteristicsValue = htons(Config_TLV_InterfaceIsLoopback_Value);
    }
    return (sizeof(*characteristicsTLV) + sizeof(*characteristicsValue));
}

uint64_t setPerfCounterTLV(void *buffer, uint64_t offset){
    generic_tlv_t *perf = (generic_tlv_t *) (buffer+offset);
    uint32_t *perfValue = (void *) (perf + sizeof(*perf));
    perf->TLVType       = tlv_perfCounterFrequency;
    perf->TLVLength     = sizeof(*perfValue);
   *perfValue           = htonl(1000000);
    return sizeof(*perf)+sizeof(*perfValue);
}

uint64_t setIconImageTLV(void *buffer, uint64_t offset){
    generic_tlv_t *icon = (generic_tlv_t *) (buffer+offset);
    icon->TLVType       = tlv_iconImage;
    icon->TLVLength     = 0x00;
    return sizeof(*icon);
}
uint64_t setMachineNameTLV(void *buffer, uint64_t offset){
    
}
uint64_t setSupportInfoTLV(void *buffer, uint64_t offset){
    
}
uint64_t setFriendlyNameTLV(void *buffer, uint64_t offset){
    
}
uint64_t setUuidTLV(void *buffer, uint64_t offset){
    
}
uint64_t setHardwareIdTLV(void *buffer, uint64_t offset){
    
}

//TODO: see if there really is support for Level2 Forwarding.. ? or just leave it hardcoded
uint64_t setQosCharacteristicsTLV(void *buffer, uint64_t offset){
    generic_tlv_t *QosCharacteristicsTLV = (generic_tlv_t *) (buffer + offset);
    uint16_t *qosCharacteristics         = (void *)(buffer + offset + sizeof(generic_tlv_t));
    QosCharacteristicsTLV->TLVType       = tlv_qos_characteristics;
    QosCharacteristicsTLV->TLVLength     = sizeof(*qosCharacteristics);
    *qosCharacteristics                  = htons(Config_TLV_QOS_L2Fwd | Config_TLV_QOS_PrioTag | Config_TLV_QOS_VLAN);
    return sizeof(generic_tlv_t) + sizeof(uint16_t);
}

uint64_t setDetailedIconTLV(void *buffer, uint64_t offset){
    
}
uint64_t setComponentTableTLV(void *buffer, uint64_t offset){
    
}
uint64_t setEndOfPropertyTLV(void *buffer, uint64_t offset){
    uint8_t *eop  = (uint8_t *) (buffer + offset);
    *eop = eofpropmarker;
    return sizeof (*eop);
}

#pragma mark -
#pragma mark Interface Specific TLVs
uint64_t setPhysicalMediumTLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *hdr = (generic_tlv_t *) (buffer + offset);

    hdr->TLVType = tlv_ifType;
    hdr->TLVLength = sizeof(uint32_t);

    uint32_t *ifType = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));
    *ifType          = htonl((uint32_t)currentNetworkInterface->ifType);
    return sizeof(generic_tlv_t) + sizeof(uint32_t);
}

uint64_t setIPv4TLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *hdr = (generic_tlv_t *) (buffer + offset);
    hdr->TLVType = tlv_ipv4;
    hdr->TLVLength = sizeof(uint32_t);
    
    uint32_t *ipv4 = (uint32_t *)(buffer + offset + sizeof(generic_tlv_t));
    *ipv4 = currentNetworkInterface->IPv4Addr;
    
    return sizeof (*hdr) + sizeof(uint32_t);
}

uint64_t setIPv6TLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    generic_tlv_t *hdr = (generic_tlv_t *) (buffer + offset);
    
    struct in6_addr *bla = (buffer + offset + sizeof(*hdr));
    *bla = currentNetworkInterface->IPv6Addr;
    
    hdr->TLVType = tlv_ipv6;
    hdr->TLVLength = sizeof(currentNetworkInterface->IPv6Addr);
    
    return sizeof (*hdr) + hdr->TLVLength;
}

uint64_t setLinkSpeedTLV(void *buffer, uint64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    uint32_t *linkSpeedValue = (buffer + offset + sizeof(generic_tlv_t));
    generic_tlv_t *linkSpeedTL = (generic_tlv_t *) (buffer + offset);
    linkSpeedTL->TLVType   = tlv_linkSpeed;
    linkSpeedTL->TLVLength = sizeof(*linkSpeedValue);
    
    *linkSpeedValue = htonl((uint32_t)(currentNetworkInterface->LinkSpeed)/100);
    
    return (sizeof(*linkSpeedTL) + sizeof(*linkSpeedValue));
}


uint64_t setSeeslistWorkingSetTLV(void *buffer, uint64_t offset){
    
}
#pragma mark -
#pragma mark 802.11 Interface Specific TLVs
uint64_t setWirelessTLV(void *buffer, uint64_t offset){
    return 0;
}

uint64_t setComponentTable(void *buffer, uint64_t offset){
    
    return 0;
}


uint64_t setBSSIDTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t setSSIDTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t setWifiMaxRateTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t setWifiRssiTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t set80211MediumTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t setAPAssociationTableTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t setRepeaterAPLineageTLV(void *buffer, uint64_t offset){
    return 0;
}
uint64_t setRepeaterAPTableTLV(void *buffer, uint64_t offset){
    return 0;
}
