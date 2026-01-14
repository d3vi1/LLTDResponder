/******************************************************************************
 *                                                                            *
 *   lltdTlvOps.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 26.04.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDd_tlv_ops_h
#define LLTDd_tlv_ops_h

static const ethernet_address_t EthernetBroadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

bool compareEthernetAddress      (const ethernet_address_t *A, const ethernet_address_t *B);
size_t setLltdHeader             (void *buffer, ethernet_address_t *source, ethernet_address_t *destination, uint16_t seqNumber, uint8_t opcode, uint8_t tos);
size_t setLltdHeaderEx           (void *buffer, const ethernet_address_t *ethSource, const ethernet_address_t *ethDest, const ethernet_address_t *realSource, const ethernet_address_t *realDest, uint16_t seqNumber, uint8_t opcode, uint8_t tos);
size_t setHelloHeader            (void *buffer, uint64_t offset, ethernet_address_t *apparentMapper, ethernet_address_t *currentMapper, uint16_t generation);
size_t setHostnameTLV            (void *buffer, uint64_t offset);
size_t setPerfCounterTLV         (void *buffer, uint64_t offset);
size_t setIconImageTLV           (void *buffer, uint64_t offset);
size_t setMachineNameTLV         (void *buffer, uint64_t offset);
size_t setSupportInfoTLV         (void *buffer, uint64_t offset);
size_t setFriendlyNameTLV        (void *buffer, uint64_t offset);
size_t setUuidTLV                (void *buffer, uint64_t offset);
size_t setHardwareIdTLV          (void *buffer, uint64_t offset);
size_t setQosCharacteristicsTLV  (void *buffer, uint64_t offset);
size_t setDetailedIconTLV        (void *buffer, uint64_t offset);
size_t setComponentTableTLV      (void *buffer, uint64_t offset);
size_t setEndOfPropertyTLV       (void *buffer, uint64_t offset);
size_t setWirelessTLV            (void *buffer, uint64_t offset, void *networkInterface);
size_t setComponentTable         (void *buffer, uint64_t offset);
size_t setBSSIDTLV               (void *buffer, uint64_t offset, void *networkInterface);
size_t setSSIDTLV                (void *buffer, uint64_t offset, void *networkInterface);
size_t setWifiMaxRateTLV         (void *buffer, uint64_t offset, void *networkInterface);
size_t setWifiRssiTLV            (void *buffer, uint64_t offset, void *networkInterface);
size_t set80211MediumTLV         (void *buffer, uint64_t offset, void *networkInterface);
size_t setAPAssociationTableTLV  (void *buffer, uint64_t offset, void *networkInterface);
size_t setRepeaterAPLineageTLV   (void *buffer, uint64_t offset, void *networkInterface);
size_t setRepeaterAPTableTLV     (void *buffer, uint64_t offset, void *networkInterface);
size_t setHostIdTLV              (void *buffer, uint64_t offset, void *networkInterface);
size_t setCharacteristicsTLV     (void *buffer, uint64_t offset, void *networkInterface);
size_t setPhysicalMediumTLV      (void *buffer, uint64_t offset, void *networkInterface);
size_t setIPv4TLV                (void *buffer, uint64_t offset, void *networkInterface);
size_t setIPv6TLV                (void *buffer, uint64_t offset, void *networkInterface);
size_t setLinkSpeedTLV           (void *buffer, uint64_t offset, void *networkInterface);

#endif
