/******************************************************************************
 *                                                                            *
 *   esxi-ops.h                                                               *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_ESXI_OPS_H
#define LLTDD_ESXI_OPS_H

#include <stddef.h>
#include <stdbool.h>

void getMachineName(char **data, size_t *stringSize);
void getFriendlyName(char **data, size_t *stringSize);
void getSupportInfo(void **data, size_t *stringSize);
void getIconImage(void **icon, size_t *iconsize);
void getUpnpUuid(void **data);
void getHwId(void *data);
void getDetailedIconImage(void **data, size_t *iconsize);
void getHostCharacteristics(void *data);
void getComponentTable(void **data, size_t *dataSize);
void setPromiscuous(void *currentNetworkInterface, boolean_t set);
boolean_t getWifiMode(void *currentNetworkInterface);
boolean_t getBSSID(void **data, void *currentNetworkInterface);

#endif /* LLTDD_ESXI_OPS_H */
