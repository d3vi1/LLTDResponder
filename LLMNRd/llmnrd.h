/******************************************************************************
 *                                                                            *
 *   llmnrd.h                                                                 *
 *   LLMNRd                                                                   *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/

#ifndef LLMNRd_llmnrd_h
#define LLMNRd_llmnrd_h

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IONetworkMedium.h>                  // For link status and speed
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <SystemConfiguration/SCNetwork.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>     // For IP Configuration
#include <SystemConfiguration/SCNetworkConnection.h>        // For Connection status
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h> // For DHCPInfoGetOptionData
#include <mach/mach.h>                                      // For RunLoop Notifications
#include <CoreServices/CoreServices.h>                      // For UTIIdentification
#include <ImageIO/ImageIO.h>                                // For ICNS to ICO conversion using Thumbnails
#include <asl.h>                                            // Apple System Logging instead of printf()
#include <launch.h>                                         // LaunchD Notification
#include <net/if.h>                                     // For IFFlags
#include "darwin-ops.h"



#pragma mark -
typedef struct {
    io_object_t            notification;
    CFStringRef            deviceName;
    uint8_t                hwAddress [ kIOEthernetAddressSize ];
    uint32_t               ifType;              // The generic kernel interface type
                                                // (csma/cd applies to ethernet/firware
                                                // and wireless, although they are
                                                // different and wireless is CSMA/CA
    CFStringRef            interfaceType;       // A string describing the interface
                                                // type (Ethernet/Firewire/IEEE80211)
    SCNetworkInterfaceRef  SCNetworkInterface;
    SCNetworkConnectionRef SCNetworkConnection;
    CFNumberRef            flags;               // kIOInterfaceFlags from the Interface
    CFNumberRef            linkStatus;          // kIOLinkStatus from the Controller
    //TODO: Add the pthread struct here in case we need to stop the thread
} network_interface_t;

IONotificationPortRef notificationPort;
CFRunLoopRef          runLoop;
aslclient             asl;
aslmsg                log_msg;

void deviceAppeared(void *refCon, io_iterator_t iterator);
void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);
void validateInterface(void *refCon, io_service_t IONetworkInterface);

#define ETHERNET_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERNET_ADDR(x) x[0], x[1], x[2], x[3], x[4], x[5]
#define lltdEtherType 0x88D9
#define lltdOUI 0x000D3A

#endif
