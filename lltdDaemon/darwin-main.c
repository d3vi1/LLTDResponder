/******************************************************************************
 *                                                                            *
 *   darwin-main.c                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright © 2014 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/


#include "lltdDaemon.h"

void SignalHandler(int Signal) {
    log_debug("Interrupted by signal #%d", Signal);
    printf("\nInterrupted by signal #%d\n", Signal);
    exit(0);
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
    CFNumberRef           CFLinkStatus;
    
    
    //
    // Get the object's parent (the interface controller) in order to get the
    // Mac Address from it.
    //
    log_debug("%s Getting the interface controller", currentNetworkInterface->deviceName);
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
    
    if(IONetworkController) {
        CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
        
        
        CFDataGetBytes((CFDataRef)macAddressAsData, CFRangeMake(0,CFDataGetLength(macAddressAsData)), currentNetworkInterface->macAddress);
        CFRelease(macAddressAsData);
        CFLinkStatus = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkStatus), kCFAllocatorDefault, 0);
        
        CFTypeRef ioLinkSpeed = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkSpeed), kCFAllocatorDefault, 0);
        if (ioLinkSpeed) {
            CFNumberGetValue((CFNumberRef)ioLinkSpeed, kCFNumberLongLongType, &currentNetworkInterface->LinkSpeed);
            CFRelease(ioLinkSpeed);
        } else {
            log_err("%s Could not read ifLinkSpeed.", currentNetworkInterface->deviceName);
        }
    }
    
    IOObjectRelease(IONetworkController);

    //
    // We attempt to connect to the System Configuration Framework in order to
    // get the SCNetworkConnection for this particular interface. First step:
    // get a copy of all the interfaces in the scInterfaces array.
    //
    
    CFArrayRef          scInterfaces;
    uint32              Index;

    log_debug("%s Getting the System Configuration Framework Interfaces", currentNetworkInterface->deviceName);
	scInterfaces = SCNetworkInterfaceCopyAll();
	if (scInterfaces == NULL) {
        log_err("%s Could not find any SCNetworkInterfaces. Is configd running?", currentNetworkInterface->deviceName);
        return;
    }
    
    //
    // Go through the array, get the current interface SCNetwork
    // and save it in the currentNetworkInterface struct
    //
    log_debug("%s Matching the interface to the SCNetworkInterfaces entry.", currentNetworkInterface->deviceName);
    for (Index = 0; Index < CFArrayGetCount(scInterfaces); Index++) {
		SCNetworkInterfaceRef	currentSCInterface;
        const char            * scDeviceName = NULL;
        
		currentSCInterface = CFArrayGetValueAtIndex(scInterfaces, Index);
        

		while ((currentSCInterface != NULL) && (scDeviceName == NULL)) {
			scDeviceName = CFStringGetCStringPtr(SCNetworkInterfaceGetBSDName(currentSCInterface),kCFStringEncodingUTF8);
			if (scDeviceName == NULL) currentSCInterface = SCNetworkInterfaceGetInterface(currentSCInterface);
		}
        if (scDeviceName)
		if (!strcmp(currentNetworkInterface->deviceName, scDeviceName)) {
            
			if (!currentNetworkInterface->SCNetworkInterface) {
				currentNetworkInterface->SCNetworkInterface = currentSCInterface;
                CFRetain(currentSCInterface);
			} else {
				// if multiple interfaces match
				currentNetworkInterface->SCNetworkInterface = NULL;
				log_err("%s Multiple scInterfaces match", currentNetworkInterface->deviceName);
			}
		}
        
	}
    
    CFRelease(scInterfaces);

    //
    // Get the default information about the interface.
    //

    CFStringRef scDeviceTypeRef = SCNetworkInterfaceGetInterfaceType(currentNetworkInterface->SCNetworkInterface);
    const char *scDeviceType    = malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(scDeviceTypeRef), kCFStringEncodingUTF8));
    CFStringGetCString(scDeviceTypeRef, (char *)scDeviceType, CFStringGetLength(scDeviceTypeRef)+1, kCFStringEncodingUTF8);
    
    if (scDeviceType){
        if(!strcmp(scDeviceType, CFStringGetCStringPtr(kSCNetworkInterfaceTypeBond, kCFStringEncodingUTF8))) {
            currentNetworkInterface->interfaceType = NetworkInterfaceTypeBond;
            log_debug("%s The SCNetworkInterfaces Type is SCNetworkInterfaceTypeBond", currentNetworkInterface->deviceName);
        } else if(!strcmp(scDeviceType, CFStringGetCStringPtr(kSCNetworkInterfaceTypeVLAN, kCFStringEncodingUTF8))) {
            currentNetworkInterface->interfaceType = NetworkInterfaceTypeVLAN;
            log_debug("%s The SCNetworkInterfaces Type is SCNetworkInterfaceTypeVLAN", currentNetworkInterface->deviceName);
        } else if(!strcmp(scDeviceType, CFStringGetCStringPtr(kSCNetworkInterfaceTypeEthernet, kCFStringEncodingUTF8))) {
            currentNetworkInterface->interfaceType = NetworkInterfaceTypeEthernet;
            log_debug("%s The SCNetworkInterfaces Type is SCNetworkInterfaceTypeEthernet", currentNetworkInterface->deviceName);
        } else if(!strcmp(scDeviceType, CFStringGetCStringPtr(kSCNetworkInterfaceTypeIEEE80211, kCFStringEncodingUTF8))){
            currentNetworkInterface->interfaceType = NetworkInterfaceTypeIEEE80211;
            log_debug("%s The SCNetworkInterfaces Type is SCNetworkInterfaceTypeIEEE80211", currentNetworkInterface->deviceName);
        } else {
            log_err("%s The SCNetworkInterfaces Type is not one of BOND, VLAN, Bridge, Ethernet or 802.11", currentNetworkInterface->deviceName);
            return;
        }
        CFRelease(scDeviceTypeRef);
    }
    
    log_debug("%s Getting the interface type string", currentNetworkInterface->deviceName);
    CFTypeRef ioInterfaceType = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceType), kCFAllocatorDefault, 0);
    if (ioInterfaceType) {
        CFNumberGetValue((CFNumberRef)ioInterfaceType, kCFNumberLongType, &currentNetworkInterface->ifType);
        CFRelease(ioInterfaceType);
    } else {
        log_err("%s Could not read ifType", currentNetworkInterface->deviceName);
    }

    log_debug("%s Getting the interface MTU", currentNetworkInterface->deviceName);
    CFTypeRef ioInterfaceMTU = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOMaxTransferUnit), kCFAllocatorDefault, 0);
    if (ioInterfaceMTU) {
        CFNumberGetValue((CFNumberRef)ioInterfaceMTU, kCFNumberLongType, &currentNetworkInterface->MTU);
        CFRelease(ioInterfaceMTU);
    } else {
        log_err("%s Could not read ifMaxTransferUnit", currentNetworkInterface->deviceName);
    }

    free((void *)scDeviceType);

    //
    // Check if we're UP, BROADCAST and not LOOPBACK
    //
    log_debug("%s Checking the flags (UP&BROADCAST) !LOOPBACK", currentNetworkInterface->deviceName);
    CFNumberRef CFFlags = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceFlags), kCFAllocatorDefault, kNilOptions);
    if (CFFlags!=NULL){
        int64_t         flags = 0;
        CFNumberGetValue(CFFlags, kCFNumberIntType, &flags);
        currentNetworkInterface->flags = flags;

        if( (!(currentNetworkInterface->flags & (IFF_UP | IFF_BROADCAST))) | (currentNetworkInterface->flags & IFF_LOOPBACK)){
            log_err("%s Failed the flags check", currentNetworkInterface->deviceName);
            CFRelease(CFFlags);
            return;
        } else CFRelease(CFFlags);
    }
    
    
    //
    //Let's get the Medium Type
    //
    log_debug("%s Getting the medium type", currentNetworkInterface->deviceName);
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
    log_debug("%s Checking for link", currentNetworkInterface->deviceName);

    int  LinkStatus = 0;
    bool hasLink = FALSE;
    CFNumberGetValue(CFLinkStatus, kCFNumberIntType, &LinkStatus);
    currentNetworkInterface->linkStatus=LinkStatus;
    
    if((LinkStatus & (kIONetworkLinkValid|kIONetworkLinkActive))
				   == (kIONetworkLinkValid|kIONetworkLinkActive)){
        log_debug("%s Interface has a Valid and Active Link", currentNetworkInterface->deviceName);
    } else if (LinkStatus & kIONetworkLinkActive) {
        log_debug("%s Validation Failed: Interface doesn't have a Valid Link but has an Active one", currentNetworkInterface->deviceName);
        return;
    } else if (LinkStatus & kIONetworkLinkValid) {
        log_debug("%s Validation Failed: Interface doesn't have an Active Link but has a Valid one", currentNetworkInterface->deviceName);
        return;
    }
    
    log_debug("%s Getting the IPvX addresses", currentNetworkInterface->deviceName);
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
            if (! strcmp(currentNetworkInterface->deviceName, temp_addr->ifa_name)) {
            
                if(temp_addr->ifa_addr->sa_family == AF_INET && !foundIPv4) {
                    currentNetworkInterface->IPv4Addr = ((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr.s_addr;
                    foundIPv4 = true;
                }
                if(temp_addr->ifa_addr->sa_family == AF_INET6 && !foundIPv6) {
                    currentNetworkInterface->IPv6Addr = ((struct sockaddr_in6 *)temp_addr->ifa_addr)->sin6_addr;
                    foundIPv6 = true;
                }
            }
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
    log_debug("%s Add Service Interest Notification", currentNetworkInterface->deviceName);
    kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
    
    
    if (kernel_return!=KERN_SUCCESS) log_err("%s IOServiceAddInterestNofitication Interface error: 0x%08x.", currentNetworkInterface->deviceName, kernel_return);
    

    //
    // Get the object's parent (the interface controller) in order to watch the
    // controller for link state changes and add that to the runLoop
    // notifications list. The desired message is kIOMessageServicePropertyChange
    // and it affects kIOLinkStatus.
    //
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
    
    if(IONetworkController) {

        kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
        if (kernel_return!=KERN_SUCCESS) log_err("%s IOServiceAddInterestNofitication Controller error: 0x%08x.", currentNetworkInterface->deviceName, kernel_return);

        
    }
    
    IOObjectRelease(IONetworkController);

    log_info("%s Found interface:   MAC: "ETHERNET_ADDR_FMT" Type: %s (0x%000x)", currentNetworkInterface->deviceName, ETHERNET_ADDR(currentNetworkInterface->macAddress), scDeviceType, currentNetworkInterface->ifType);
    
    
    //
    // After documenting the interface inside our currentNetworkInterface we
    // know that it's up, broadcast capable, not a loopback and has a link
    // So we move on to adding the thread
    //
    log_debug("%s Spawning the thread", currentNetworkInterface->deviceName);
    pthread_attr_t  threadAttributes;
    pthread_t       posixThreadID;
    int             returnVal;
    
    returnVal = pthread_attr_init(&threadAttributes);
    
    if (returnVal!=noErr){
        log_err("%s Could not init pthread attributes: %d", currentNetworkInterface->deviceName, returnVal);
        return;
    }
    
    returnVal = pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_DETACHED);
    if (returnVal!=noErr){
        log_err("%s Could not set pthread attributes: %d", currentNetworkInterface->deviceName, returnVal);
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
    io_name_t nameString;
    IORegistryEntryGetName(service, nameString);
    log_notice("Notification received: %s type: %x device: %s", currentNetworkInterface->deviceName, messageType, nameString);
    if (messageType == kIOMessageServiceIsTerminated) {
        log_notice("Interface removed: %s", currentNetworkInterface->deviceName);
        free((void *)currentNetworkInterface->deviceName);
        CFRelease(currentNetworkInterface->SCNetworkInterface);
        IOObjectRelease(currentNetworkInterface->notification);
        pthread_cancel(currentNetworkInterface->posixThreadID);
        if(currentNetworkInterface->recvBuffer) free(currentNetworkInterface->recvBuffer);
        free(currentNetworkInterface);
    } else if (messageType == kIOMessageDeviceWillPowerOff) {
        log_notice("Interface powered off: %s", currentNetworkInterface->deviceName);
        CFRelease(currentNetworkInterface->SCNetworkInterface);
        pthread_kill(currentNetworkInterface->posixThreadID, 0);
    } else if (messageType == kIOMessageServicePropertyChange) {
        //We should have some cleanup over here. There are only two properties that we care about.
        log_notice("A property has changed. Reloading thread. %s", currentNetworkInterface->deviceName);
        if (currentNetworkInterface->SCNetworkInterface) CFRelease(currentNetworkInterface->SCNetworkInterface);
        if (currentNetworkInterface->SCNetworkConnection) CFRelease(currentNetworkInterface->SCNetworkConnection);
        if (currentNetworkInterface->seelist) CFRelease(currentNetworkInterface->seelist);
        currentNetworkInterface->SCNetworkInterface=NULL;
        currentNetworkInterface->SCNetworkConnection=NULL;
        currentNetworkInterface->seelist=NULL;
        pthread_cancel(currentNetworkInterface->posixThreadID);
        usleep(100000);
        if(currentNetworkInterface->recvBuffer) {
            free(currentNetworkInterface->recvBuffer);
            currentNetworkInterface->recvBuffer=NULL;
        }
        close(currentNetworkInterface->socket);
        validateInterface(currentNetworkInterface, service);
    } else if (messageType == kIOMessageDeviceWillPowerOn) {
        log_notice("Interface powered on: %s", currentNetworkInterface->deviceName);
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
        CFStringRef deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        if (deviceName == NULL) {
            usleep(100000);
            deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        }
        
        currentNetworkInterface->deviceName=malloc(CFStringGetLength(deviceName)+1);
        CFStringGetCString(deviceName, (char *)currentNetworkInterface->deviceName, CFStringGetLength(deviceName)+1, kCFStringEncodingUTF8);
        CFRelease(deviceName);
        log_debug("Validating the interface");
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
    log_debug("Setting up interrupt handlers.");
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) log_crit("Could not establish SIGINT handler.");
    handler = signal(SIGTERM, SignalHandler);
    if (handler == SIG_ERR) log_crit("Could not establish SIGTERM handler.");
    
    
    //
    // Creates and returns a notification object for receiving
    // IOKit notifications of new devices or state changes
    //
    log_debug("Setting up notification port.");
    masterPort = MACH_PORT_NULL;
    kernel_return = IOMasterPort(bootstrap_port, &masterPort);
    if (kernel_return != KERN_SUCCESS) {
        log_err("IOMasterPort returned 0x%x", kernel_return);
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
    log_debug("Setting up service notification for kIONetworkInterfaceClass.");
    kernel_return = IOServiceAddMatchingNotification(notificationPort, kIOFirstMatchNotification, IOServiceMatching(kIONetworkInterfaceClass), deviceAppeared, NULL, &newDevicesIterator);
    if (kernel_return!=KERN_SUCCESS) log_crit("IOServiceAddMatchingNotification(deviceAppeared) returned 0x%x", kernel_return);
    
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
    log_notice("Starting run loop.\n");
    CFRunLoopRun();
    
    //
    // We should never get here
    //
    log_crit("Unexpectedly back from CFRunLoopRun()!");
    if (masterPort != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), masterPort);
    asl_close(asl);
    return EXIT_FAILURE;
    
}
