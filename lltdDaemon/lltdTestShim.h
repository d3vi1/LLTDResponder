#ifndef LLTD_TEST_SHIM_H
#define LLTD_TEST_SHIM_H

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "lltdBlock.h"

#ifndef KERN_SUCCESS
#define KERN_SUCCESS 0
#endif

#ifndef IFM_FDX
#define IFM_FDX 0x0010
#endif

typedef bool boolean_t;

typedef struct {
    const char *deviceName;
    uint32_t    ifType;
    enum { NetworkInterfaceTypeBond, NetworkInterfaceTypeBridge, NetworkInterfaceTypeEthernet, NetworkInterfaceTypeIEEE80211, NetworkInterfaceTypeVLAN} interfaceType;
    uint32_t    MediumType;
    uint32_t    flags;
    uint32_t    LinkSpeed;
    uint8_t     macAddress[6];
} network_interface_t;

#define lltdEtherType 0x88D9

void getMachineName(char **name, size_t *size);
void getSupportInfo(void **info, size_t *size);
void getUpnpUuid(void **uuid);
uint8_t getWifiMode(void *networkInterface);
bool getBSSID(void *bssid, void *networkInterface);
bool getSSID(char **ssid, size_t *size, void *networkInterface);
bool getWifiMaxRate(uint32_t *rate, void *networkInterface);
bool getWifiRssi(int8_t *rssi, void *networkInterface);
bool getWifiPhyMedium(uint32_t *medium, void *networkInterface);

#endif
