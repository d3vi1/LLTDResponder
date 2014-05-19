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
    
    lltd_hello_upper_header_t *helloHeader = (lltd_hello_upper_header_t *) buffer+offset;

    memcpy(&helloHeader->apparentMapper, apparentMapper, sizeof(ethernet_address_t));
    memcpy(&helloHeader->currentMapper, currentMapper, sizeof(ethernet_address_t));
    helloHeader->generation = generation;
    
    return sizeof(lltd_hello_upper_header_t);
}

#pragma mark -
#pragma mark Host specific TLVs
void setHostnameTLV(){
    
}
void setCharacteristicsTLV(){
    
}
void setPerfCounterTLV(){
    
}
void setIconImageTLV(){
    
}
void setMachineNameTLV(){
    
}
void setSupportInfoTLV(){
    
}
void setFriendlyNameTLV(){
    
}
void setUuidTLV(){
    
}
void setHardwareIdTLV(){
    
}
void setQosCharacteristicsTLV(){
    
}
void setDetailedIconTLV(){
    
}
void setComponentTableTLV(){
    
}
void setEndOfPropertyTLV(){
    
}

#pragma mark -
#pragma mark Interface Specific TLVs
void setPhysicalMediumTLV(void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    uint8_t Type = tlv_ifType;
    uint8_t Length = 4;
    uint32_t ifType = currentNetworkInterface->ifType;
}
void setIPv4TLV(void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    uint8_t Type = tlv_ifType;
    uint8_t Length = 4;
    uint32_t ifType = currentNetworkInterface->ifType;
}
void setIPv6TLV(){
    
}
void setLinkSpeedTLV(){
    
}
void setSeeslistWorkingSetTLV(){
    
}
#pragma mark -
#pragma mark 802.11 Interface Specific TLVs
void setWirelessTLV(){
    
}
void setBSSIDTLV(){
    
}
void setSSIDTLV(){
    
}
void setWifiMaxRateTLV(){
    
}
void setWifiRssiTLV(){
    
}
void set80211MediumTLV(){
    
}
void setAPAssociationTableTLV(){
    
}
void setRepeaterAPLineageTLV(){
    
}
void setRepeaterAPTableTLV(){
    
}
