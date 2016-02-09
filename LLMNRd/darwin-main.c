/******************************************************************************
 *                                                                            *
 *   llmnrd.c                                                                 *
 *   LLMNRd                                                                   *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/


#include "lltdDaemon.h"
#define debug 1

void SignalHandler(int Signal) {
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "\nInterrupted by signal #%d\n", Signal);
    printf("\nInterrupted by signal #%d\n", Signal);
    exit(0);
}

void print_ip(int ip) {
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    printf("%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
}

//==============================================================================
//
// We are checking if it's an Ethernet Interface, if it's up, if it has an
// IPv4 stack and if it has an IPv6 stack. If all are met, we are updating the
// networkInterface object object with that information and starting listner
// threads on each of the IP addresses of the interface that we can safely
// kill when we get a ServiceIsTerminated kernel message.
//
//==============================================================================
void validateInterface(void *refCon, io_service_t IONetworkInterface) {
    kern_return_t         kernel_return;
    io_service_t          IONetworkController;
    network_interface_t  *currentNetworkInterface = (network_interface_t*) refCon;
    
    
    //
    // Get the object's parent (the interface controller) in order to get the
    // Mac Address from it.
    //
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not get the parent of the interface\n", __FUNCTION__);
    
    if(IONetworkController) {
        CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
        
        
        CFDataGetBytes((CFDataRef)macAddressAsData, CFRangeMake(0,CFDataGetLength(macAddressAsData)), currentNetworkInterface->hwAddress);
        CFRelease(macAddressAsData);
        currentNetworkInterface->linkStatus = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkStatus), kCFAllocatorDefault, 0);
        
        CFTypeRef ioLinkSpeed = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkSpeed), kCFAllocatorDefault, 0);
        if (ioLinkSpeed) {
            CFNumberGetValue((CFNumberRef)ioLinkSpeed, kCFNumberLongLongType, &currentNetworkInterface->LinkSpeed);
            CFRelease(ioLinkSpeed);
        } else {
            asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not read ifLinkSpeed.\n", __FUNCTION__);
        }
    }
    
    // We're not releasing the controller here because
    IOObjectRelease(IONetworkController);

    //
    // We attempt to connect to the System Configuration Framework in order to
    // get the SCNetworkConnection for this particular interface. First step:
    // get a copy of all the interfaces in the scInterfaces array.
    //
    
    CFArrayRef          scInterfaces;
    uint32              Index;
    
	scInterfaces = SCNetworkInterfaceCopyAll();
	if (scInterfaces == NULL) {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: could not find any SCNetworkInterfaces. Is configd running?\n", __FUNCTION__);
        return;
    }
    
    //
    // Go through the array, get the current interface SCNetwork
    // and save it in the currentNetworkInterface struct
    //
    for (Index = 0; Index < CFArrayGetCount(scInterfaces); Index++) {
		SCNetworkInterfaceRef	currentSCInterface;
        CFStringRef             scDeviceName	= NULL;
        
		currentSCInterface = CFArrayGetValueAtIndex(scInterfaces, Index);
		while ((currentSCInterface != NULL) && (scDeviceName == NULL)) {
			scDeviceName = SCNetworkInterfaceGetBSDName(currentSCInterface);
			if (scDeviceName == NULL) currentSCInterface = SCNetworkInterfaceGetInterface(currentSCInterface);
		}
        
		if ((scDeviceName != NULL) && CFEqual(currentNetworkInterface->deviceName, scDeviceName)) {
			if (currentNetworkInterface->SCNetworkInterface == NULL) {
				currentNetworkInterface->SCNetworkInterface = currentSCInterface;
                CFRetain(currentSCInterface);
			} else {
				// if multiple interfaces match
				currentNetworkInterface->SCNetworkInterface = NULL;
				asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: multiple scInterfaces match\n", __FUNCTION__);
			}
		}
	}
    
    CFRelease(scInterfaces);

    //
    // Get the default information about the interface
    //
    currentNetworkInterface->interfaceType=SCNetworkInterfaceGetInterfaceType(currentNetworkInterface->SCNetworkInterface);
    CFTypeRef ioInterfaceType = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceType), kCFAllocatorDefault, 0);
    if (ioInterfaceType) {
        CFNumberGetValue((CFNumberRef)ioInterfaceType, kCFNumberLongType, &currentNetworkInterface->ifType);
        CFRelease(ioInterfaceType);
    } else {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not read ifType.\n", __FUNCTION__);
    }

    CFTypeRef ioInterfaceMTU = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOMaxTransferUnit), kCFAllocatorDefault, 0);
    if (ioInterfaceMTU) {
        CFNumberGetValue((CFNumberRef)ioInterfaceMTU, kCFNumberLongType, &currentNetworkInterface->MTU);
        CFRelease(ioInterfaceMTU);
    } else {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not read ifMaxTransferUnit.\n", __FUNCTION__);
    }


#ifdef debug
    //
    // DEBUG: Print the network interfaces to stdout
    //
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Found interface:   %s  MAC: "ETHERNET_ADDR_FMT"  Type: %s (0x%000x)\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8), ETHERNET_ADDR(currentNetworkInterface->hwAddress), CFStringGetCStringPtr(currentNetworkInterface->interfaceType, kCFStringEncodingUTF8), currentNetworkInterface->ifType);
#endif
    
    //
    // Check if we're UP, BROADCAST and not LOOPBACK
    //
    currentNetworkInterface->flags = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceFlags), kCFAllocatorDefault, kNilOptions);
    if (currentNetworkInterface->flags!=NULL){
        uint32_t flags;
        CFNumberGetValue(currentNetworkInterface->flags, kCFNumberIntType, &flags);
        //FIXME: There are way too many things here
        if(!( (flags & IFF_UP) && (flags & IFF_BROADCAST) && (!(flags & IFF_LOOPBACK)))){
            asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: %s failed the flags check\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8));
            return;
        }
    }
    
    //
    //Let's get the Medium Type
    //
    CFDictionaryRef properties = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMediumDictionary), kCFAllocatorDefault, kNilOptions);
    if (properties!=NULL){
        CFNumberRef activeMediumIndex = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOActiveMedium), kCFAllocatorDefault, kNilOptions);
        if (activeMediumIndex!=NULL) {
            CFDictionaryRef activeMedium = (CFDictionaryRef)CFDictionaryGetValue(properties, activeMediumIndex);
            if (activeMedium!=NULL){
                uint64_t mediumType;
                CFNumberRef mediumTypeCF = CFDictionaryGetValue(activeMedium, CFSTR(kIOMediumType));
                CFNumberGetValue( mediumTypeCF, kCFNumberLongLongType, &mediumType);
                currentNetworkInterface->MediumType = mediumType;
                CFRetain(mediumType);
                CFRelease(mediumTypeCF);
                CFRelease(activeMedium);
            }
            CFRelease(activeMediumIndex);
        }
        CFRelease(properties);
    }
    //
    // Check if we have link on the controller
    //
    if (currentNetworkInterface->linkStatus!=NULL){
        uint32_t linkStatus;
        CFNumberGetValue(currentNetworkInterface->linkStatus, kCFNumberIntType, &linkStatus);
        //FIXME: There are way too many things here
        if(!( (linkStatus & kIONetworkLinkActive) && (linkStatus & kIONetworkLinkValid) )){
            asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: %s failed link status check\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8));
            return;
        }
    }
    
    //Let's get the IPv4 (and IPv6?) address(es?)
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    success = getifaddrs(&interfaces);
    
    boolean_t foundIPv4 = false;
    boolean_t foundIPv6 = false;
    
    if (success == 0) {
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            char *ifName = malloc(16);
            CFStringGetCString(currentNetworkInterface->deviceName,ifName, 16,kCFStringEncodingUTF8);
            if (! strcmp(ifName, temp_addr->ifa_name)) {
            
                if(temp_addr->ifa_addr->sa_family == AF_INET && !foundIPv4) {
                    currentNetworkInterface->IPv4Addr = ((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr.s_addr;
                    foundIPv4 = true;
                }
                if(temp_addr->ifa_addr->sa_family == AF_INET6 && !foundIPv6) {
                    currentNetworkInterface->IPv6Addr = ((struct sockaddr_in6 *)temp_addr->ifa_addr)->sin6_addr;
                    foundIPv6 = true;
                }
            }
            free(ifName);
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);

    
    //
    // Add a a notification for anything happening to the device to the
    // run loop. The actual event gets filtered in the deviceDisappeared
    // function. We are also giving it a pointer to our networkInterface
    // so that we don't keep a global array of all interfaces.
    //
    kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
    
    
    if (kernel_return!=KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: IOServiceAddInterestNofitication Interface error: 0x%08x.\n", __FUNCTION__, kernel_return);
    

    //
    // Get the object's parent (the interface controller) in order to watch the
    // controller for link state changes and add that to the runLoop
    // notifications list. The desired message is kIOMessageServicePropertyChange
    // and it affects kIOLinkStatus.
    //
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not get the parent of the interface\n", __FUNCTION__);
    
    if(IONetworkController) {

        kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
        if (kernel_return!=KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: IOServiceAddInterestNofitication Controller error: 0x%08x.\n", __FUNCTION__, kernel_return);

        
    }
    
    IOObjectRelease(IONetworkController);

    
    //
    // After documenting the interface inside our currentNetworkInterface we
    // know that it's up, broadcast capable, not a loopback and has a link
    // So we move on to adding the thread
    //
    pthread_attr_t  threadAttributes;
    pthread_t       posixThreadID;
    int             returnVal;
    
    returnVal = pthread_attr_init(&threadAttributes);
    
    if (returnVal!=noErr){
        asl_log(asl,log_msg, ASL_LEVEL_ERR, "%s:, could not init pthread attributes: %d\n", __FUNCTION__, returnVal);
        return;
    }
    returnVal = pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_DETACHED);
    if (returnVal!=noErr){
        asl_log(asl,log_msg, ASL_LEVEL_ERR, "%s:, could not set pthread attributes: %d\n", __FUNCTION__, returnVal);
        return;
    }
    
    int threadError = pthread_create(&posixThreadID, &threadAttributes, (void *)&lltdBlock, currentNetworkInterface);
    
    currentNetworkInterface->posixThreadID = posixThreadID;
    returnVal = pthread_attr_destroy(&threadAttributes);
    
    assert(!returnVal);
    if (threadError != 0)
    {
        // TODO: Report an error.
    }
    
    
    //
    // TODO: Clean-up the SC stuff
    //
    
}


//==============================================================================
//
// Print out the device that died on us to srdout, clean-up the notification and
// the network_interface_t object.
//
//==============================================================================
void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument){
    
    network_interface_t *currentNetworkInterface = (network_interface_t*) refCon;
    
    if (messageType == kIOMessageServiceIsTerminated) {
        asl_log(asl, log_msg, ASL_LEVEL_NOTICE, "%s: Interface removed: %s\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8));
        CFRelease(currentNetworkInterface->deviceName);
        CFRelease(currentNetworkInterface->interfaceType);
        CFRelease(currentNetworkInterface->SCNetworkInterface);
        IOObjectRelease(currentNetworkInterface->notification);
        free(currentNetworkInterface);
        pthread_kill(currentNetworkInterface->posixThreadID, 0);
    } else if (messageType == kIOMessageServicePropertyChange) {
        asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: A property has changed.\n", __FUNCTION__);
        //Let's release the interface and validate it again.
        CFRelease(currentNetworkInterface->interfaceType);
        CFRelease(currentNetworkInterface->SCNetworkInterface);
        IOObjectRelease(currentNetworkInterface->notification);
        pthread_kill(currentNetworkInterface->posixThreadID, 0);
        validateInterface(currentNetworkInterface, service);
        
        
    }
}


//==============================================================================
//
// We identify the network interface and set the IOKit interest notification
// for any events regarding to it!
//
//==============================================================================
void deviceAppeared(void *refCon, io_iterator_t iterator){

    kern_return_t      kernel_return;
    io_service_t       IONetworkInterface;
    
    //
    // Let's loop through the interfaces that we've got
    //
    while ((IONetworkInterface = IOIteratorNext(iterator))){

        //
        // Let's initialize the networkInterface Data Structure
        //
        network_interface_t *currentNetworkInterface = NULL;
        currentNetworkInterface = malloc(sizeof(network_interface_t));
        bzero(currentNetworkInterface,sizeof(network_interface_t));
        
        //
        // Let's get the device name. If we don't have a BSD name, we are
        // letting it stabilize for 50ms since SCNetwork didn't do it's thing
        // yet.
        //
        currentNetworkInterface->deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        if (currentNetworkInterface->deviceName == NULL) {
            usleep(100000);
            currentNetworkInterface->deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        }
        
        validateInterface(currentNetworkInterface, IONetworkInterface);
        
        //
        // Clean-up.
        //
        IOObjectRelease(IONetworkInterface);
        
    } // while ((IONetworkInterface = IOIteratorNext(iterator))

}

//==============================================================================
//
// main
// TODO: Convert to a Launch Daemon
//
//==============================================================================
int main(int argc, const char *argv[]){
    sig_t                 handler;
    kern_return_t         kernel_return;
    mach_port_t           masterPort;
    CFRunLoopSourceRef    runLoopSource;
    io_iterator_t         newDevicesIterator;
    
    //
    // Create a new Apple System Log facility entry
    // of Daemon type with the LLMNR name
    //
    asl = NULL;
    log_msg = NULL;
    asl = asl_open("LLTD", "Daemon", ASL_OPT_STDERR);
    log_msg = asl_new(ASL_TYPE_MSG);
    asl_set(log_msg, ASL_KEY_SENDER, "LLTD");

    
    //
    // Set up a signal handler so we can clean up when we're interrupted from
    // the command line. Otherwise we stay in our run loop forever.
    // SigInt = Ctrl+C
    // SigTerm = Killed by launchd
    //
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) asl_log(asl, log_msg, ASL_LEVEL_CRIT, "Could not establish SIGINT handler.\n");
    handler = signal(SIGTERM, SignalHandler);
    if (handler == SIG_ERR) asl_log(asl, log_msg, ASL_LEVEL_CRIT, "Could not establish SIGTERM handler.\n");
    
    
    //
    // Creates and returns a notification object for receiving
    // IOKit notifications of new devices or state changes
    //
    masterPort = MACH_PORT_NULL;
    kernel_return = IOMasterPort(bootstrap_port, &masterPort);
    if (kernel_return != KERN_SUCCESS) {
        asl_log(asl, log_msg, ASL_LEVEL_CRIT, "IOMasterPort returned 0x%x\n", kernel_return);
        return EXIT_FAILURE;
    }
    
    //
    // Let's get the Run Loop ready. It needs a notification
    // port and a clean iterator.
    //
    newDevicesIterator = 0;
    notificationPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(notificationPort);
    runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopDefaultMode);

    //
    // Add the Service Notification to the Run Loop. It will
    // get the loop moving we we get a device added notification.
    // The device removed notifications are added in the Run Loop
    // from the actual "deviceAppeared" function.
    //
    kernel_return = IOServiceAddMatchingNotification(notificationPort, kIOFirstMatchNotification, IOServiceMatching(kIONetworkInterfaceClass), deviceAppeared, NULL, &newDevicesIterator);
    if (kernel_return!=KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_CRIT, "IOServiceAddMatchingNotification(deviceAppeared) returned 0x%x\n", kernel_return);
    
    //
    // Do an initial device scan since the Run Loop is not yet
    // started. This will also add the notifications:
    // * Device disappeared
    // * IOKit Property changed
    // * From IOKit PowerManagement we get the sleep/hibernate
    //   shutdown and wake notifications (NOTYET);
    // * From SystemConfiguration we get the IPv4/IPv6 add,
    //   change, removed notifications (NOTYET);
    //
    deviceAppeared(NULL, newDevicesIterator);

    
    //
    // Start the run loop.
    //
    asl_log(asl, log_msg, ASL_LEVEL_NOTICE, "Starting run loop.\n\n");
    CFRunLoopRun();
    
    //
    // We should never get here
    //
    asl_log(asl, log_msg, ASL_LEVEL_CRIT, "Unexpectedly back from CFRunLoopRun()!\n");
    if (masterPort != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), masterPort);
    asl_close(asl);
    return EXIT_FAILURE;
    
}
