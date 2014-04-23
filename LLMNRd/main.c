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
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "\nInterrupted by signal #%d\n", Signal);
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
    
    if (kernel_return != KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not get the parent of the interface\n", __FUNCTION__);
    
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
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not read ifType.\n", __FUNCTION__);
    }
    //
    // DEBUG: Print the network interfaces to stdout
    //
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Found interface:   %s  MAC: %02x:%02x:%02x:%02x:%02x:%02x  Type: %s (0x%000x)\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8),currentNetworkInterface->hwAddress[0], currentNetworkInterface->hwAddress[1], currentNetworkInterface->hwAddress[2], currentNetworkInterface->hwAddress[3], currentNetworkInterface->hwAddress[4], currentNetworkInterface->hwAddress[5], CFStringGetCStringPtr(currentNetworkInterface->interfaceType, kCFStringEncodingUTF8), currentNetworkInterface->ifType);

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
        
        
        if (kernel_return!=KERN_SUCCESS) asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: IOServiceAddInterestNofitication error: 0x%08x.\n", __FUNCTION__, kernel_return);
        
        //
        // Clean-up.
        //
        IOObjectRelease(IONetworkInterface);
        
    } // while ((IONetworkInterface = IOIteratorNext(iterator))
}


//==============================================================================
//
// main
// TODO: Convert to daemon with ASL logging
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
    asl = asl_open("LLMNR", "Daemon", ASL_OPT_STDERR);
    log_msg = asl_new(ASL_TYPE_MSG);
    asl_set(log_msg, ASL_KEY_SENDER, "LLMNR");

    
/*  //
    // Register ourselves with LaunchD
    //
    launch_data_t sockets_dict, checkin_response;
    launch_data_t checkin_request;
    launch_data_t listening_fd_array;
    if ((checkin_request = launch_data_new_string(LAUNCH_KEY_CHECKIN)) == NULL) {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "launch_data_new_string(\"" LAUNCH_KEY_CHECKIN "\") Unable to create string.");
        asl_close(asl);
        return EXIT_FAILURE;
    }
    
    if ((checkin_response = launch_msg(checkin_request)) == NULL) {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "launch_msg(\"" LAUNCH_KEY_CHECKIN "\") IPC failure: %m");
        asl_close(asl);
        return EXIT_FAILURE;
    }
    
    if (LAUNCH_DATA_ERRNO == launch_data_get_type(checkin_response)) {
        errno = launch_data_get_errno(checkin_response);
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "Check-in failed: %m");
        asl_close(asl);
        return EXIT_FAILURE;
    }
    
    launch_data_t the_label = launch_data_dict_lookup(checkin_response, LAUNCH_JOBKEY_LABEL);
    if (NULL == the_label) {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "No label found");
        asl_close(asl);
        return EXIT_FAILURE;
    }

    asl_log(asl, log_msg, ASL_LEVEL_NOTICE, "Label: %s", launch_data_get_string(the_label));
*/
    
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
    // started. This will also add the following notifications:
    //
    // * From IOKit we get the serviceTerminated for removal;
    // * From IOKit PowerManagement we get the sleep/hibernate
    //   shutdown and wake notifications (NOTYET);
    // * From SystemConfiguration we get the IPv4/IPv6 add,
    //   change, removed notifications (NOTYET);
    // * From SystemConfiguration we get the link
    //   notifications (NOTYET);
    // * From CoreWLAN we get the connected/disconnected
    //   notifications, though SC might provide them also(NOTYET);
    //
    //TODO: Add the notifications mentioned above.
    deviceAppeared(NULL, newDevicesIterator);

    
    //
    // XXX: Testing the functions in darwin-ops. Not for production.
    //
#ifdef debug

    void *hostname = NULL;
    size_t sizeOfHostname = 0;
    void *friendlyName = NULL;
    size_t sizeOfFriendlyHostname = 0;
    void *iconImage = NULL;
    size_t sizeOfIconImage = 0;
    void *supportURL = NULL;
    size_t sizeOfSupportURL = 0;
    getMachineName((char **)&hostname, &sizeOfHostname);
    getFriendlyName((char **)&friendlyName, &sizeOfFriendlyHostname);
    getIconImage(&iconImage, &sizeOfIconImage);
    getSupportInfo(&supportURL, &sizeOfSupportURL);
    // While the wprintf doesn't work, watching the buffer shows
    // that the strings are there and in UCS-2 format.
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "getHostname: %ls\n", (wchar_t *)hostname);
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "getFriendlyName: %ls\n", (wchar_t *)friendlyName);
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "getIconImage result: %d bytes\n", (int)sizeOfIconImage);
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "getSupportURL: %s\n", (char *)supportURL);
    free(hostname);
    free(friendlyName);
    free(iconImage);
    free(supportURL);
#endif
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
