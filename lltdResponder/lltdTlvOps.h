/******************************************************************************
 *                                                                            *
 *   lltdTlvOps.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 26.04.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTD_RESPONDER_TLV_OPS_H
#define LLTD_RESPONDER_TLV_OPS_H

#include <stddef.h>

#include "lltdProtocol.h"

size_t setHostnameTLV            (void *buffer, size_t offset);
size_t setPerfCounterTLV         (void *buffer, size_t offset);
size_t setIconImageTLV           (void *buffer, size_t offset);
size_t setSupportInfoTLV         (void *buffer, size_t offset);
size_t setFriendlyNameTLV        (void *buffer, size_t offset);
size_t setUuidTLV                (void *buffer, size_t offset);
size_t setHardwareIdTLV          (void *buffer, size_t offset);
size_t setQosCharacteristicsTLV  (void *buffer, size_t offset);
size_t setDetailedIconTLV        (void *buffer, size_t offset);
size_t setComponentTableTLV      (void *buffer, size_t offset);
size_t setEndOfPropertyTLV       (void *buffer, size_t offset);
size_t setWirelessTLV            (void *buffer, size_t offset, void *iface_ctx);
size_t setComponentTable         (void *buffer, size_t offset);
size_t setBSSIDTLV               (void *buffer, size_t offset, void *iface_ctx);
size_t setSSIDTLV                (void *buffer, size_t offset, void *iface_ctx);
size_t setWifiMaxRateTLV         (void *buffer, size_t offset, void *iface_ctx);
size_t setWifiRssiTLV            (void *buffer, size_t offset, void *iface_ctx);
size_t set80211MediumTLV         (void *buffer, size_t offset, void *iface_ctx);
size_t setAPAssociationTableTLV  (void *buffer, size_t offset, void *iface_ctx);
size_t setRepeaterAPLineageTLV   (void *buffer, size_t offset, void *iface_ctx);
size_t setRepeaterAPTableTLV     (void *buffer, size_t offset, void *iface_ctx);
size_t setHostIdTLV              (void *buffer, size_t offset, void *iface_ctx);
size_t setCharacteristicsTLV     (void *buffer, size_t offset, void *iface_ctx);
size_t setPhysicalMediumTLV      (void *buffer, size_t offset, void *iface_ctx);
size_t setIPv4TLV                (void *buffer, size_t offset, void *iface_ctx);
size_t setIPv6TLV                (void *buffer, size_t offset, void *iface_ctx);
size_t setLinkSpeedTLV           (void *buffer, size_t offset, void *iface_ctx);

#endif
