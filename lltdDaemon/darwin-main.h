/******************************************************************************
 *                                                                            *
 *   darwin-main.h                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright © 2014 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDd_darwin_h
#define LLTDd_darwin_h

#pragma mark -

#pragma pack( push )
#pragma pack( 2 )

#pragma pack( pop )
/*
extern const CFStringRef kSCNetworkInterfaceTypeBond
extern const CFStringRef kSCNetworkInterfaceTypeEthernet
extern const CFStringRef kSCNetworkInterfaceTypeFireWire
extern const CFStringRef kSCNetworkInterfaceTypeIEEE80211
extern const CFStringRef kSCNetworkInterfaceTypeVLAN
extern const CFStringRef kSCNetworkInterfaceTypeWWAN
 */
typedef struct {
    io_object_t            notification;
    const char            *deviceName;
    uint32_t               ifType;              // The generic kernel interface type
                                                // (csma/cd applies to ethernet/firware
                                                // and wireless, although they are
                                                // different and wireless is CSMA/CA
    enum { NetworkInterfaceTypeBond, NetworkInterfaceTypeEthernet, NetworkInterfaceTypeIEEE80211, NetworkInterfaceTypeVLAN} interfaceType;
    SCNetworkInterfaceRef  SCNetworkInterface;
    SCNetworkConnectionRef SCNetworkConnection;
    int64_t                flags;               // kIOInterfaceFlags from the Interface
    uint64_t               linkStatus;          // kIOLinkStatus from the Controller
    uint32_t               MTU;                 // We'll set the buffer size to the MTU size
    uint64_t               MediumType;          // Get the current medium Type
    uint64_t               LinkSpeed;           // The current link speed, automatically updated when the property changes
    uint32_t               IPv4Addr;
    int                    socket;
    struct sockaddr_ndrv   socketAddr;
    struct in6_addr        IPv6Addr;
    pthread_t              posixThreadID;
    void                  *icon;
    size_t                 iconSize;
    CFMutableArrayRef      seelist;
    uint16_t               MapperSeqNumber;
    uint8_t                macAddress [ kIOEthernetAddressSize ];
    uint8_t                MapperHwAddress[ kIOEthernetAddressSize ];
    void                  *recvBuffer;          //We need to clear the receive Buffer when we kill the thread.

} network_interface_t;

IONotificationPortRef       notificationPort;
CFRunLoopRef                runLoop;
aslclient                   asl;
aslmsg                      log_msg;

void deviceAppeared   (void *refCon, io_iterator_t iterator);
void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);
void validateInterface(void *refCon, io_service_t IONetworkInterface);

#define PRINT_IP_FMT      "%i.%i.%i.%i"
#define PRINT_IP(x)       (x >> 24) & 0xFF, (x >> 16) & 0xFF, (x >> 8) & 0xFF, x & 0xFF
#define ETHERNET_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERNET_ADDR(x)  x[0], x[1], x[2], x[3], x[4], x[5]
#define lltdEtherType     0x88D9
#define lltdOUI           0x000D3A

#endif
