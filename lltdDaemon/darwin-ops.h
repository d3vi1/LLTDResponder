/******************************************************************************
 *                                                                            *
 *   darwin-ops.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 24.03.2014.                      *
 *   Copyright © 2014 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDd_darwin_ops_h
#define LLTDd_darwin_ops_h

#pragma mark Functions that return machine information

void getMachineName                (char **data, size_t *stringSize);
void getFriendlyName               (char **data, size_t *stringSize);
void getSupportInfo                (void **data, size_t *stringSize);
void getIconImage                  (void **icon, size_t *iconsize);
void getUpnpUuid                   (void **data);
void getHwId                       (void *data);
void getDetailedIconImage          (void *data);// Only with QueryLargeTLV
void getHostCharacteristics        (void *data);
void getComponentTable             (void *data);// Only with QueryLargeTLV
void getPerformanceCounterFrequency(void *data);
void setPromiscuous                (void *currentNetworkInterface, boolean_t set);

#pragma mark Functions that are interface specific
#pragma mark -

//gets the IPv4 address if one is available
//boolean getIfIPv4info
//boolean getIfIPv6info
//Type 0x2 uint8, Length 0x2 uint8, Bits: public, private, duplex, hasmanagement, loopback, reserved[11]
//boolean getIfCharacteristics
//boolean getQosCharacteristics
//gets the IANAifType
//void getIfPhysicalMedium
//void WgetWirelessMode
//void WgetIfRSSI
//void WgetIfSSID
//void WgetIfBSSID
//void WgetIfMaxRate
//void WgetIfPhyMedium
//void WgetApAssociationTable

#endif
