//
//  llmnrd.h
//  LLMNRd
//
//  Created by Răzvan Corneliu C.R. VILT on 16.03.2014.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//

#ifndef LLMNRd_llmnrd_h
#define LLMNRd_llmnrd_h

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <mach/mach.h>

typedef struct networkInterface {
    io_object_t     notification;
    CFStringRef     deviceName;
    uint8_t         MACAddress [ kIOEthernetAddressSize ];
    uint8_t         InterfaceType;
} networkInterface;

IONotificationPortRef notificationPort;
CFRunLoopRef          runLoop;

void deviceAppeared(void *refCon, io_iterator_t iterator);
void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);
boolean_t validateInterface(void *refCon);

#endif
