/******************************************************************************
 *                                                                            *
 *   darwin-main.h                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDd_darwin_h
#define LLTDd_darwin_h

#pragma mark -

#pragma pack( push )
#pragma pack( 2 )

#pragma pack( pop )

struct {
    void                  *smallIcon;
    size_t                 smallIconSize;
    void                  *largeIcon;
    size_t                 largeIconSize;
    IONotificationPortRef  notificationPort;
    CFRunLoopRef           runLoop;
    aslclient              asl;
    aslmsg                 log_msg;
} globalInfo;

typedef struct automata automata;

typedef struct {
    io_object_t            notification;
    const char            *deviceName;
    uint32_t               ifType;              // The generic kernel interface type
                                                // (csma/cd applies to ethernet/firware
                                                // and wireless, although they are
                                                // different and wireless is CSMA/CA
    enum { NetworkInterfaceTypeBond, NetworkInterfaceTypeBridge, NetworkInterfaceTypeEthernet, NetworkInterfaceTypeIEEE80211, NetworkInterfaceTypeVLAN} interfaceType;

    //We can get these by ioctls:
    //IFRTYPE_FAMILY_ETHERNET(IFRTYPE_SUBFAMILY_ANY,IFRTYPE_SUBFAMILY_USB,IFRTYPE_SUBFAMILY_BLUETOOTH,IFRTYPE_SUBFAMILY_WIFI,IFRTYPE_SUBFAMILY_THUNDERBOLT), IFRTYPE_FAMILY_VLAN, IFRTYPE_FAMILY_BOND, IFRTYPE_FAMILY_BRIDGE
    
    int64_t                flags;               // kIOInterfaceFlags from the Interface
    uint64_t               linkStatus;          // kIOLinkStatus from the Controller
    uint32_t               MTU;                 // We'll set the buffer size to the MTU size
    uint64_t               MediumType;          // Get the current medium Type
    uint64_t               LinkSpeed;           // The current link speed, automatically updated when the property changes
    int                    socket;
    struct sockaddr_ndrv   socketAddr;
    pthread_t              posixThreadID;
    void                  *seeList;
    uint32_t               seeListCount;
    uint16_t               MapperSeqNumber;
    uint8_t                macAddress      [ kIOEthernetAddressSize ];
    uint8_t                MapperHwAddress [ kIOEthernetAddressSize ];
    void                  *recvBuffer;          //We need to clear the receive Buffer when we kill the thread.
    automata              *mappingAutomata;
    automata              *sessionAutomata;
    automata              *enumerationAutomata;
} network_interface_t;

void deviceAppeared   (void *refCon, io_iterator_t iterator);
void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);
void validateInterface(void *refCon, io_service_t IONetworkInterface);

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define PRINT_IP_FMT      "%i.%i.%i.%i"
#define PRINT_IP(x)       (x >> 24) & 0xFF, (x >> 16) & 0xFF, (x >> 8) & 0xFF, x & 0xFF
#define ETHERNET_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERNET_ADDR(x)  x[0], x[1], x[2], x[3], x[4], x[5]
#define lltdEtherType     0x88D9
#define lltdOUI           0x000D3A

#endif
