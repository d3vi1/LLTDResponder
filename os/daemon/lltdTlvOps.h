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

#include "../../lltdResponder/lltdWire.h"

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
