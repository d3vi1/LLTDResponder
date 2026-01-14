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

// Forward declarations for automata types
struct automata;
struct session_table;

typedef struct {
    const char *deviceName;
    uint32_t    ifType;
    uint32_t    MediumType;
    uint32_t    flags;
    uint64_t    LinkSpeed;
    uint8_t     macAddress[6];
    uint8_t     MapperHwAddress[6];
    uint8_t     MapperApparentAddress[6];
    uint8_t     MapperKnown;
    uint16_t    MapperSeqNumber;
    uint16_t    MapperGenerationTopology;
    uint16_t    MapperGenerationQuick;
    uint64_t    LastHelloTxMs;
    uint64_t    LastHelloReplyMs;
    uint16_t    LastHelloReplyXid;
    uint16_t    LastHelloReplyGen;
    uint8_t     LastHelloReplyTos;
    int         socket;
    uint32_t    MTU;
    void       *seeList;
    uint32_t    seeListCount;
    void       *recvBuffer;
    struct automata *mappingAutomata;
    struct automata *sessionAutomata;
    struct automata *enumerationAutomata;
    struct session_table *sessionTable;
    struct sockaddr_ndrv {
        unsigned char snd_len;
        unsigned char snd_family;
        char snd_name[12];
    } socketAddr;
    int         helloSent;
} network_interface_t;

#ifndef lltdEtherType
#define lltdEtherType 0x88D9
#endif

// Host information functions
void getMachineName(char **name, size_t *size);
void getSupportInfo(void **info, size_t *size);
void getUpnpUuid(void **uuid);
void getHwId(void *data);
void getDetailedIconImage(void **data, size_t *iconsize);
void getComponentTable(void **data, size_t *dataSize);

// WiFi interface functions
uint8_t getWifiMode(void *networkInterface);
bool getBSSID(void *bssid, void *networkInterface);
bool getSSID(char **ssid, size_t *size, void *networkInterface);
bool getWifiMaxRate(uint32_t *rate, void *networkInterface);
bool getWifiRssi(int8_t *rssi, void *networkInterface);
bool getWifiPhyMedium(uint32_t *medium, void *networkInterface);

#endif
