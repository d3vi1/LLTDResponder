/******************************************************************************
 *                                                                            *
 *   linux-ops.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 02.03.2016.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDd_linux_ops_h
#define LLTDd_linux_ops_h

#include <stddef.h>
#include <stdint.h>

void getMachineName(char **data, size_t *stringSize);
void getFriendlyName(char **data, size_t *stringSize);
void getSupportInfo(void **data, size_t *stringSize);
void getIconImage(void **icon, size_t *iconsize);
void getUpnpUuid(void **data);
void getHwId(void *data);
void getDetailedIconImage(void **data, size_t *iconsize); // Only with QueryLargeTLV
void getHostCharacteristics(void *data);
void getComponentTable(void **data, size_t *dataSize); // Only with QueryLargeTLV
void getPerformanceCounterFrequency(void *data);

void setPromiscuous(void *currentNetworkInterface, boolean_t set);

boolean_t getWifiMode(void *currentNetworkInterface);
boolean_t getBSSID(void **data, void *currentNetworkInterface);
boolean_t getSSID(char **ssid, size_t *size, void *currentNetworkInterface);
boolean_t getWifiMaxRate(uint32_t *rateMbps, void *currentNetworkInterface);
boolean_t getWifiRssi(int8_t *rssi, void *currentNetworkInterface);
boolean_t getWifiPhyMedium(uint32_t *phyMedium, void *currentNetworkInterface);

#endif
