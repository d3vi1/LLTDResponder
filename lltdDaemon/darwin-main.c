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
    // Mac Address, Link Status, Link Speed and Active Medium from it.
    //
    log_debug("%s Getting the interface controller", currentNetworkInterface->deviceName);
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
    
    if(IONetworkController) {
        CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
        
        
        CFDataGetBytes((CFDataRef)macAddressAsData, CFRangeMake(0,CFDataGetLength(macAddressAsData)), currentNetworkInterface->macAddress);
        CFRelease(macAddressAsData);
        CFLinkStatus = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkStatus), kCFAllocatorDefault, 0);
        
        if (CFLinkStatus){
            //
            // Check if we have link on the controller
            //
            log_debug("%s Checking for link", currentNetworkInterface->deviceName);
            int  LinkStatus = 0;
            bool hasLink = FALSE;
            CFNumberGetValue(CFLinkStatus, kCFNumberIntType, &LinkStatus);
            currentNetworkInterface->linkStatus=LinkStatus;
        
            if((LinkStatus & (kIONetworkLinkValid|kIONetworkLinkActive)) == (kIONetworkLinkValid|kIONetworkLinkActive)){
                log_debug("%s Interface has a Valid and Active Link", currentNetworkInterface->deviceName);
            } else if (LinkStatus & kIONetworkLinkActive) {
                log_debug("%s Validation Failed: Interface doesn't have a Valid Link but has an Active one", currentNetworkInterface->deviceName);
                return;
            } else if (LinkStatus & kIONetworkLinkValid) {
                log_debug("%s Validation Failed: Interface doesn't have an Active Link but has a Valid one", currentNetworkInterface->deviceName);
                return;
            }
            CFRelease(CFLinkStatus);
        } else {
            log_err("%s Could not read linkStatus.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            return;
        }
        
        //
        // Let's get the Medium Speed
        CFTypeRef ioLinkSpeed = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkSpeed), kCFAllocatorDefault, 0);
        if (ioLinkSpeed) {
            CFNumberGetValue((CFNumberRef)ioLinkSpeed, kCFNumberLongLongType, &currentNetworkInterface->LinkSpeed);
            CFRelease(ioLinkSpeed);
        } else {
            log_err("%s Could not read ifLinkSpeed.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            return;
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
                    CFRelease(mediumTypeCF);
                    CFRelease(activeMedium);
                }
                CFRelease(activeMediumIndex);
            }
            CFRelease(properties);
        } else {
            log_err("%s Could not read Active Medium Type.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            return;
        }
    }
    
    IOObjectRelease(IONetworkController);

    
    //
    // Get the interface MTU.
    //
    log_debug("%s Getting the interface MTU", currentNetworkInterface->deviceName);
    CFTypeRef ioInterfaceMTU = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOMaxTransferUnit), kCFAllocatorDefault, 0);
    if (ioInterfaceMTU) {
        CFNumberGetValue((CFNumberRef)ioInterfaceMTU, kCFNumberLongType, &currentNetworkInterface->MTU);
        CFRelease(ioInterfaceMTU);
    } else {
        log_err("%s Could not read ifMaxTransferUnit", currentNetworkInterface->deviceName);
    }


    //
    // Check if we're UP, BROADCAST and not LOOPBACK.
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
    // Get the Interface Type
    //
    if ( IOObjectConformsTo( IONetworkInterface, "IO80211Interface" ) ) {
        currentNetworkInterface->interfaceType=NetworkInterfaceTypeIEEE80211;
    } else if ( IOObjectConformsTo( IONetworkInterface, "IOEthernetInterface" ) ) {
        currentNetworkInterface->interfaceType=NetworkInterfaceTypeEthernet;
    } else {
        currentNetworkInterface->interfaceType=NetworkInterfaceTypeEthernet;
    }
    
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
    
    if (threadError != KERN_SUCCESS) log_err("%s Could not launch thread with error %d.", currentNetworkInterface->deviceName, threadError);
    

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
        IOObjectRelease(currentNetworkInterface->notification);
        pthread_cancel(currentNetworkInterface->posixThreadID);
        if(currentNetworkInterface->recvBuffer) free(currentNetworkInterface->recvBuffer);
        free(currentNetworkInterface);
    } else if (messageType == kIOMessageDeviceWillPowerOff) {
        log_notice("Interface powered off: %s", currentNetworkInterface->deviceName);
        pthread_kill(currentNetworkInterface->posixThreadID, 0);
    } else if (messageType == kIOMessageServicePropertyChange) {
        //We should have some cleanup over here. There are only two properties that we care about.
        log_notice("A property has changed. Reloading thread. %s", currentNetworkInterface->deviceName);
        if (currentNetworkInterface->seeList) {
            free(currentNetworkInterface->seeList);
            currentNetworkInterface->seeList=NULL;
        }
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
