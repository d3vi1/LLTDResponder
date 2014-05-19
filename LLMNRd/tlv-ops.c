//
//  tlv-ops.c
//  LLMNRd
//
//  Created by Răzvan Corneliu C.R. VILT on 26.04.2014.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//
#include    "tlv-ops.h"
//
#pragma mark -
#pragma mark Header generation

typedef struct {
    uint8_t TLVType;
    uint8_t TLVLength;
}  generic_tlv_t;

typedef struct {
    uint8_t     TLVType;
    uint8_t     TLVLength;
    uint16_t    ConfigTLV;
} characteristic_tlv_t;

u_int64_t setLltdHeader (void *buffer, ethernet_address_t *source, ethernet_address_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos){
    
    lltd_demultiplex_header_t *lltdHeader = (lltd_demultiplex_header_t*) buffer;
    
    lltdHeader->frameHeader.ethertype = lltdEtherType;
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

bool compareEthernetAddress(ethernet_address_t *A, ethernet_address_t *B) {
    if ((A->a[0]==B->a[0])&&(A->a[1]==B->a[1])&&(A->a[2]==B->a[2])&&(A->a[3]==B->a[3])&&(A->a[4]==B->a[4])&&(A->a[5]==B->a[5])) return true;
    return false;
}

//
// Get the mess out of lltdBlock.c
//
u_int64_t setHelloHeader (void *buffer, u_int64_t offset, ethernet_address_t *apparentMapper, ethernet_address_t *currentMapper, uint16_t generation){
    
    lltd_hello_upper_header_t *helloHeader = (lltd_hello_upper_header_t *) (buffer+offset);

    memcpy(&helloHeader->apparentMapper, apparentMapper, sizeof(ethernet_address_t));
    memcpy(&helloHeader->currentMapper, currentMapper, sizeof(ethernet_address_t));
    helloHeader->generation = generation;
    
    return sizeof(lltd_hello_upper_header_t);
}

#pragma mark -
#pragma mark Host specific TLVs
u_int64_t setHostnameTLV(void *buffer, u_int64_t offset){
    void *hostname = NULL;
    size_t sizeOfHostname = 0;
    getMachineName((char **)&hostname, &sizeOfHostname);
    
    generic_tlv_t *hostnameTLV = (generic_tlv_t *) (buffer+offset);
    hostnameTLV->TLVType = tlv_hostname;
    hostnameTLV->TLVLength = sizeOfHostname;
    void *hostNameStringOffset = (buffer+offset+sizeof(hostnameTLV));
    memcpy(hostNameStringOffset, hostname, sizeOfHostname);
    free(hostname);
    
    return sizeof(hostnameTLV)+sizeOfHostname;
}
//FIXME: mAREE de citit full duplex sau nu, NAT sau nu.. bla bla
u_int64_t setCharacteristicsTLV(void *buffer, u_int64_t offset) {
    characteristic_tlv_t * charac = (characteristic_tlv_t *) (buffer+offset);
    uint32_t characteristicsValue;
    charac->TLVType = tlv_characterisics;
    charac->TLVLength = sizeof(characteristicsValue);
    characteristicsValue = characteristicsValue | Config_TLV_NetworkInterfaceDuplex_Value;
    return sizeof(characteristic_tlv_t);
}
u_int64_t setPerfCounterTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setIconImageTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setMachineNameTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setSupportInfoTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setFriendlyNameTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setUuidTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setHardwareIdTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setQosCharacteristicsTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setDetailedIconTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setComponentTableTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setEndOfPropertyTLV(void *buffer, u_int64_t offset){
    
}

#pragma mark -
#pragma mark Interface Specific TLVs
u_int64_t setPhysicalMediumTLV(void *buffer, u_int64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    uint8_t Type = tlv_ifType;
    uint8_t Length = 4;
    uint32_t ifType = currentNetworkInterface->ifType;
    
    memcpy(buffer+offset, ifType, sizeof(ifType));
    
    return sizeof(ifType);
}
u_int64_t setIPv4TLV(void *buffer, u_int64_t offset, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    uint8_t Type = tlv_ifType;
    uint8_t Length = 4;
    uint32_t ifType = currentNetworkInterface->ifType;
}
u_int64_t setIPv6TLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setLinkSpeedTLV(void *buffer, u_int64_t offset){
    
}
u_int64_t setSeeslistWorkingSetTLV(void *buffer, u_int64_t offset){
    
}
#pragma mark -
#pragma mark 802.11 Interface Specific TLVs
u_int64_t setWirelessTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setBSSIDTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setSSIDTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setWifiMaxRateTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setWifiRssiTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t set80211MediumTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setAPAssociationTableTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setRepeaterAPLineageTLV(void *buffer, u_int64_t offset){
    return 0;
}
u_int64_t setRepeaterAPTableTLV(void *buffer, u_int64_t offset){
    return 0;
}
