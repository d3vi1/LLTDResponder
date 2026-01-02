/******************************************************************************
 *                                                                            *
 *   darwin-ops.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 24.03.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
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
void getDetailedIconImage          (void **data, size_t *iconsize);// Only with QueryLargeTLV
void getHostCharacteristics        (void *data);
void getComponentTable             (void **data, size_t *dataSize);// Only with QueryLargeTLV
void getPerformanceCounterFrequency(void *data);
void setPromiscuous                (void *currentNetworkInterface, boolean_t set);
boolean_t getWifiMode              (void *currentNetworkInterface);
boolean_t getBSSID                 (void **data, void *currentNetworkInterface);
boolean_t getSSID                  (char **data, size_t *dataSize, void *currentNetworkInterface);
boolean_t getWifiMaxRate           (uint32_t *rateMbps, void *currentNetworkInterface);
boolean_t getWifiRssi              (int8_t *rssi, void *currentNetworkInterface);
boolean_t getWifiPhyMedium         (uint32_t *phyMedium, void *currentNetworkInterface);
#pragma mark Functions that are interface specific
#pragma mark -

//gets the IPv4 address if one is available
boolean_t getIfIPv4info(uint32_t *ipv4, void *currentNetworkInterface);
boolean_t getIfIPv6info(struct in6_addr *ipv6, void *currentNetworkInterface);
//Type 0x2 uint8, Length 0x2 uint8, Bits: public, private, duplex, hasmanagement, loopback, reserved[11]
boolean_t getIfCharacteristics(uint16_t *characteristics, void *currentNetworkInterface);
boolean_t getQosCharacteristics(uint16_t *characteristics);
boolean_t getIfIANAType(uint32_t *ifType, void *currentNetworkInterface);
void getIfPhysicalMedium(uint32_t *mediumType, void *currentNetworkInterface);
boolean_t WgetWirelessMode(uint8_t *mode, void *currentNetworkInterface);
boolean_t WgetIfRSSI(int8_t *rssi, void *currentNetworkInterface);
boolean_t WgetIfSSID(char **ssid, size_t *ssidSize, void *currentNetworkInterface);
boolean_t WgetIfBSSID(void **bssid, void *currentNetworkInterface);
boolean_t WgetIfMaxRate(uint32_t *rateMbps, void *currentNetworkInterface);
boolean_t WgetIfPhyMedium(uint32_t *phyMedium, void *currentNetworkInterface);
void WgetApAssociationTable(void **data, size_t *dataSize, void *currentNetworkInterface);

#endif
