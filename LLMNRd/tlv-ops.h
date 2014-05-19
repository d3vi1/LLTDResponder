//
//  tlv-ops.h
//  LLMNRd
//
//  Created by Răzvan Corneliu C.R. VILT on 26.04.2014.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//

#ifndef LLMNRd_tlv_ops_h
#define LLMNRd_tlv_ops_h
#include "llmnrd.h"
#include "darwin-ops.h"
#include "lltdBlock.h"

bool compareEthernetAddress(ethernet_address_t *A, ethernet_address_t *B);
void setLltdHeader (ethernet_address_t *source, ethernet_address_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos);
void setHelloHeader (ethernet_address_t *apparentMapper, ethernet_address_t *currentMapper, uint16_t generation);
void setHostnameTLV();
void setCharacteristicsTLV();
void setPerfCounterTLV();
void setIconImageTLV();
void setMachineNameTLV();
void setSupportInfoTLV();
void setFriendlyNameTLV();
void setUuidTLV();
void setHardwareIdTLV();
void setQosCharacteristicsTLV();
void setDetailedIconTLV();
void setComponentTableTLV();
void setEndOfPropertyTLV();
void setPhysicalMediumTLV();
void setIPv4TLV();
void setIPv6TLV();
void setLinkSpeedTLV();
void setSeeslistWorkingSetTLV();
void setWirelessTLV();
void setBSSIDTLV();
void setSSIDTLV();
void setWifiMaxRateTLV();
void setWifiRssiTLV();
void set80211MediumTLV();
void setAPAssociationTableTLV();
void setRepeaterAPLineageTLV();
void setRepeaterAPTableTLV();


#endif
