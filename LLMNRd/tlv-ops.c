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
//
void setLltdHeader (ethernet_header_t *source, ethernet_header_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos){
    
    lltdHeader->frameHeader.ethertype = lltdEtherType;
    lltdHeader->frameHeader.source.a[0] = source.a[0];
    lltdHeader->frameHeader.source.a[1] = source.a[1];
    lltdHeader->frameHeader.source.a[2] = source.a[2];
    lltdHeader->frameHeader.source.a[3] = source.a[3];
    lltdHeader->frameHeader.source.a[4] = source.a[4];
    lltdHeader->frameHeader.source.a[5] = source.a[5];
    lltdHeader->frameHeader.destination.a[0] = 0xFF;
    lltdHeader->frameHeader.destination.a[1] = 0xFF;
    lltdHeader->frameHeader.destination.a[2] = 0xFF;
    lltdHeader->frameHeader.destination.a[3] = 0xFF;
    lltdHeader->frameHeader.destination.a[4] = 0xFF;
    lltdHeader->frameHeader.destination.a[5] = 0xFF;
    lltdHeader->realSource.a[0] = source.a[0];
    lltdHeader->realSource.a[1] = source.a[1];
    lltdHeader->realSource.a[2] = source.a[2];
    lltdHeader->realSource.a[3] = source.a[3];
    lltdHeader->realSource.a[4] = source.a[4];
    lltdHeader->realSource.a[5] = source.a[5];
    lltdHeader->realDestination.a[0] = destination.a[0];
    lltdHeader->realDestination.a[1] = destination.a[1];
    lltdHeader->realDestination.a[2] = destination.a[2];
    lltdHeader->realDestination.a[3] = destination.a[3];
    lltdHeader->realDestination.a[4] = destination.a[4];
    lltdHeader->realDestination.a[5] = destination.a[5];
    lltdHeader->seqNumber = seqNumber;
    lltdHeader->opcode = opcode;
    lltdHeader->tos = tos;
    lltdHeader->version = 1;

}

//
// Get the mess out of lltdBlock.c
//
void setHelloHeader (lltd_hello_header_t *helloHeader, ethernet_header_t *apparentMapper, ethernet_header_t currentMapper, uint16_t generation){
    helloHeader->apparentMapper.a[0] = apparentMapper.a[0];
    helloHeader->apparentMapper.a[1] = apparentMapper.a[1];
    helloHeader->apparentMapper.a[2] = apparentMapper.a[2];
    helloHeader->apparentMapper.a[3] = apparentMapper.a[3];
    helloHeader->apparentMapper.a[4] = apparentMapper.a[4];
    helloHeader->apparentMapper.a[5] = apparentMapper.a[5];
    helloHeader->currentMapper.a[0] = currentMapper.a[0];
    helloHeader->currentMapper.a[1] = currentMapper.a[1];
    helloHeader->currentMapper.a[2] = currentMapper.a[2];
    helloHeader->currentMapper.a[3] = currentMapper.a[3];
    helloHeader->currentMapper.a[4] = currentMapper.a[4];
    helloHeader->currentMapper.a[5] = currentMapper.a[5];
    helloHeader->generation = generation;
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
