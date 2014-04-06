/******************************************************************************
 *                                                                            *
 *   llmnrd.c                                                                 *
 *   LLMNRd                                                                   *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/


#include "llmnrd.h"
#define debug 1

//==============================================================================
//
// We are currently checking if it's an Ethernet Interface
//
//==============================================================================
void SignalHandler(int Signal) {
    fprintf(stderr, "\nInterrupted by signal #%d\n", Signal);
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
    
    
    //
    // Get the object's parent (the interface controller) in order to get the
    // Mac Address from it.
    //
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) printf("Could not get the parent of the interface\n");
    
    if(IONetworkController) {
        
        CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
        
        
        CFDataGetBytes((CFDataRef)macAddressAsData, CFRangeMake(0,CFDataGetLength(macAddressAsData)), currentNetworkInterface->hwAddress);
        CFRelease(macAddressAsData);
        
    }
    
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
        printf("validateInterface: could not find any SCNetworkInterfaces. Is configd running?\n");
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
				printf("validateInterface: multiple scInterfaces match\n");
			}
		} else {
        }
	}
    
    CFRelease(scInterfaces);

    //
    // Get the default information about the interface
    //
    //currentNetworkInterface->ifType = 6;
    //long ifTypeTemp;
    currentNetworkInterface->interfaceType=SCNetworkInterfaceGetInterfaceType(currentNetworkInterface->SCNetworkInterface);
    CFTypeRef ioInterfaceType = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceType), kCFAllocatorDefault, 0);
    if (ioInterfaceType) {
        CFNumberGetValue((CFNumberRef)ioInterfaceType, kCFNumberLongType, &currentNetworkInterface->ifType);
        CFRelease(ioInterfaceType);
    } else {
        printf("Could not read ifType.\n");
    }
    //
    // DEBUG: Print the network interfaces to stdout
    //
    printf("Found interface:   %s  MAC: %02x:%02x:%02x:%02x:%02x:%02x  Type: %s (0x%000x)\n", CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8),currentNetworkInterface->hwAddress[0], currentNetworkInterface->hwAddress[1], currentNetworkInterface->hwAddress[2], currentNetworkInterface->hwAddress[3], currentNetworkInterface->hwAddress[4], currentNetworkInterface->hwAddress[5], CFStringGetCStringPtr(currentNetworkInterface->interfaceType, kCFStringEncodingUTF8), currentNetworkInterface->ifType);

    //TODO: Razvan: check if it's a broadcast interface and if it's up
    //TODO: Razvan: add SystemConfiguration events to the run loop.
    //TODO: Razvan: or maybe just use IOKit events for link-up/down
    //TODO: Alex: add the thread creation here
    //
    // Clean-up the SC stuff
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
    
    //
    // XXX: add parser for link-up/link-down and other SC events?!?
    //
    if (messageType == kIOMessageServiceIsTerminated) {
        printf("Interface removed: %s\n", CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8));
        
        CFRelease(currentNetworkInterface->deviceName);
        CFRelease(currentNetworkInterface->interfaceType);
        CFRelease(currentNetworkInterface->SCNetworkInterface);
        IOObjectRelease(currentNetworkInterface->notification);
        //TODO: Alex: Kill the listner thread
        free(currentNetworkInterface);
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
        // letting it stabilize for 50000 since SCNetwork didn't do it's thing
        // yet. It's a candidate for conversion from hard-coded time to
        // SystemConfiguration event monitoring.
        //
        currentNetworkInterface->deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        if (currentNetworkInterface->deviceName == NULL) {
            usleep(50000);
            currentNetworkInterface->deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        }
        
        validateInterface(currentNetworkInterface, IONetworkInterface);
        
        //
        // Add a a notification for anything happening to the device to the
        // run loop. The actual event gets filtered in the deviceDisappeared
        // function. We are also giving it a pointer to our networkInterface
        // so that we don't keep a global array of all interfaces.
        //
        kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
        
        if (kernel_return!=KERN_SUCCESS) printf("IOServiceAddInterestNofitication error: 0x%08x.\n", kernel_return);
        
        //
        // Clean-up.
        //
        IOObjectRelease(IONetworkInterface);
        
    } // while ((IONetworkInterface = IOIteratorNext(iterator))
}


//================================================================================================
//
// main
// TODO: Convert to daemon with ASL logging
//
//================================================================================================
int main(int argc, const char *argv[]){
    sig_t                 handler;
    kern_return_t         kernel_return;
    mach_port_t           masterPort;
    CFRunLoopSourceRef    runLoopSource;
    io_iterator_t         newDevicesIterator;

    //
    // Set up a signal handler so we can clean up when we're interrupted from
    // the command line. Otherwise we stay in our run loop forever.
    // SigInt = Ctrl+C
    // SigTerm = Killed by launchd
    //
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) printf("Could not establish SIGINT handler.\n");
    handler = signal(SIGTERM, SignalHandler);
    if (handler == SIG_ERR) printf("Could not establish SIGTERM handler.\n");
    
    
    //
    // Creates and returns a notification object for receiving
    // IOKit notifications of new devices or state changes
    //
    masterPort = MACH_PORT_NULL;
    kernel_return = IOMasterPort(bootstrap_port, &masterPort);
    if (kernel_return != KERN_SUCCESS) { printf("IOMasterPort returned 0x%x\n", kernel_return); return 0; }
    
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
    if (kernel_return!=KERN_SUCCESS) printf("IOServiceAddMatchingNotification(deviceAppeared) returned 0x%x\n", kernel_return);
    
    //
    // Do an initial device scan since the Run Loop is not yet
    // started. This will also add the following notifications:
    //
    // * From IOKit we get the serviceTerminated for removal;
    // * From IOKit PowerManagement we get the sleep/hibernate
    //   shutdown and wake notifications (TODO);
    // * From SystemConfiguration we get the IPv4/IPv6 add,
    //   change, removed notifications (TODO);
    // * From SystemConfiguration we get the link
    //   notifications (TODO);
    // * From CoreWLAN we get the connected/disconnected
    //   notifications (TODO);
    //
    deviceAppeared(NULL, newDevicesIterator);

    
    //
    // FIXME: Testing the functions in darwin-ops. Not for production.
    //
#ifdef debug
    //getIconImage();
    char *hostname = NULL;
    size_t sizeOfHostname = 0;
    char *friendlyName = NULL;
    size_t sizeOfFriendlyHostname = 0;
    getMachineName(&hostname, &sizeOfHostname);
    getFriendlyName(&friendlyName, &sizeOfFriendlyHostname);
    // While the wprintf doesn't work, watching the buffer shows
    // that the strings are there and in UCS-2 format.
    printf("Hostname %s\n", hostname);
    printf("FriendlyName %s\n", friendlyName);
    free(hostname);
    free(friendlyName);
#endif
    //
    // Start the run loop.
    //
    printf("Starting run loop.\n\n");
    CFRunLoopRun();
    
    //
    // We should never get here
    //
    printf("Unexpectedly back from CFRunLoopRun()!\n");
    if (masterPort != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), masterPort);
    return 0;
    
}
