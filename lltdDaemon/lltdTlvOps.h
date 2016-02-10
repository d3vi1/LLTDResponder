/******************************************************************************
 *                                                                            *
 *   lltdTlvOps.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 26.04.2014.                      *
 *   Copyright © 2014 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDd_tlv_ops_h
#define LLTDd_tlv_ops_h

#include "lltdDaemon.h"

static const ethernet_address_t EthernetBroadcast = (ethernet_address_t) {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

bool compareEthernetAddress      (const ethernet_address_t *A, const ethernet_address_t *B);
uint64_t setLltdHeader           (void *buffer, ethernet_address_t *source, ethernet_address_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos);
uint64_t setHelloHeader          (void *buffer, uint64_t offset, ethernet_address_t *apparentMapper, ethernet_address_t *currentMapper, uint16_t generation);
uint64_t setHostnameTLV          (void *buffer, uint64_t offset);
uint64_t setPerfCounterTLV       (void *buffer, uint64_t offset);
uint64_t setIconImageTLV         (void *buffer, uint64_t offset);
uint64_t setMachineNameTLV       (void *buffer, uint64_t offset);
uint64_t setSupportInfoTLV       (void *buffer, uint64_t offset);
uint64_t setFriendlyNameTLV      (void *buffer, uint64_t offset);
uint64_t setUuidTLV              (void *buffer, uint64_t offset);
uint64_t setHardwareIdTLV        (void *buffer, uint64_t offset);
uint64_t setQosCharacteristicsTLV(void *buffer, uint64_t offset);
uint64_t setDetailedIconTLV      (void *buffer, uint64_t offset);
uint64_t setComponentTableTLV    (void *buffer, uint64_t offset);
uint64_t setEndOfPropertyTLV     (void *buffer, uint64_t offset);
uint64_t setWirelessTLV          (void *buffer, uint64_t offset);
uint64_t setComponentTable       (void *buffer, uint64_t offset);
uint64_t setBSSIDTLV             (void *buffer, uint64_t offset);
uint64_t setSSIDTLV              (void *buffer, uint64_t offset);
uint64_t setWifiMaxRateTLV       (void *buffer, uint64_t offset);
uint64_t setWifiRssiTLV          (void *buffer, uint64_t offset);
uint64_t set80211MediumTLV       (void *buffer, uint64_t offset);
uint64_t setAPAssociationTableTLV(void *buffer, uint64_t offset);
uint64_t setRepeaterAPLineageTLV (void *buffer, uint64_t offset);
uint64_t setRepeaterAPTableTLV   (void *buffer, uint64_t offset);
uint64_t setHostIdTLV            (void *buffer, uint64_t offset, void *networkInterface);
uint64_t setCharacteristicsTLV   (void *buffer, uint64_t offset, void *networkInterface);
uint64_t setPhysicalMediumTLV    (void *buffer, uint64_t offset, void *networkInterface);
uint64_t setIPv4TLV              (void *buffer, uint64_t offset, void *networkInterface);
uint64_t setIPv6TLV              (void *buffer, uint64_t offset, void *networkInterface);
uint64_t setLinkSpeedTLV         (void *buffer, uint64_t offset, void *networkInterface);


#endif
