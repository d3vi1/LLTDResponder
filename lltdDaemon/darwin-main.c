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
// This is the thread that is opened for each valid interface
//
//==============================================================================
void lltdLoop (void *data){
    
    network_interface_t *currentNetworkInterface = data;
    
    int                       fileDescriptor;
    struct ndrv_protocol_desc protocolDescription;
    struct ndrv_demux_desc    demuxDescription[1];
    struct sockaddr_ndrv      socketAddress;
    
    //
    // Open the AF_NDRV RAW socket
    //
    fileDescriptor = socket(AF_NDRV, SOCK_RAW, htons(0x88D9));
    currentNetworkInterface->socket = fileDescriptor;
    
    if (fileDescriptor < 0) {
        log_err("Could not create socket on %s.", currentNetworkInterface->deviceName);
        return -1;
    }
    
    //
    // Bind to the socket
    //
    strcpy((char *)&(socketAddress.snd_name), currentNetworkInterface->deviceName);
    socketAddress.snd_len = sizeof(socketAddress);
    socketAddress.snd_family = AF_NDRV;
    if (bind(fileDescriptor, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0) {
        log_err("Could not bind socket on %s.", currentNetworkInterface->deviceName);
        return -1;
    }
    
    //
    // Create the reference to the array that will store the probes viewed on the network
    //
    
    currentNetworkInterface->seeList = NULL;
    
    //
    // Define protocol
    //
    protocolDescription.version = (u_int32_t)1;
    protocolDescription.protocol_family = lltdEtherType;
    protocolDescription.demux_count = (u_int32_t)1;
    protocolDescription.demux_list = (struct ndrv_demux_desc*)&demuxDescription;
    
    //
    // Define protocol DEMUX
    //
    demuxDescription[0].type = NDRV_DEMUXTYPE_ETHERTYPE;
    demuxDescription[0].length = 2;
    demuxDescription[0].data.ether_type = htons(lltdEtherType);
    
    //
    // Set the protocol on the socket
    //
    setsockopt(fileDescriptor, SOL_NDRVPROTO, NDRV_SETDMXSPEC, (caddr_t)&protocolDescription, sizeof(protocolDescription));
    log_notice("Successfully binded to %s", currentNetworkInterface->deviceName);
    
    //
    // Start the run loop
    //
    unsigned int cyclNo = 0;
    currentNetworkInterface->recvBuffer = malloc(currentNetworkInterface->MTU);
    
    for(;;){
        recvfrom(fileDescriptor, currentNetworkInterface->recvBuffer, currentNetworkInterface->MTU, 0, NULL, NULL);
        parseFrame(currentNetworkInterface->recvBuffer, currentNetworkInterface);
    }
    
    //
    // Cleanup
    //
    free(currentNetworkInterface->recvBuffer);
    currentNetworkInterface->recvBuffer=NULL;
    close(fileDescriptor);
    return 0;
    
}

//==============================================================================
//
// We are checking if it's an Ethernet Interface, if it's up, if it has an
// IPv4 stack and if it has an IPv6 stack. If all are met, we are updating the
// networkInterface object object with that information and starting listner
// threads on each of the IP addresses of the interface that we can safely
// kill when we get a ServiceIsTerminated kernel message.
// If the interface is valid, but otherwise fails a check for link or flags, we
// we put a notification for both the Controller and the Interface IOKit objects
// in order to be called again if something changes. It might pass the checks
// after the changes. It happens if the daemon is started before the network.
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
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    if (kernel_return != KERN_SUCCESS) log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
    
    if(IONetworkController) {
        
        
        //
        // Get the Mac Address
        //
        CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
        if(macAddressAsData) {
            CFDataGetBytes((CFDataRef)macAddressAsData, CFRangeMake(0,CFDataGetLength(macAddressAsData)), currentNetworkInterface->macAddress);
            CFRelease(macAddressAsData);
        } else {
            // Not being able to read the Mac Address is fatal and we ignore the
            // interface from here on.
            log_err("%s Could not read linkStatus.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            free((void *)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        
        
        //
        // Get/Check the LinkStatus
        //
        CFLinkStatus = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkStatus), kCFAllocatorDefault, 0);
        if (CFLinkStatus){
            /*
             * Check if we have link on the controller
             */
            int  LinkStatus = 0;
            CFNumberGetValue(CFLinkStatus, kCFNumberIntType, &LinkStatus);
            currentNetworkInterface->linkStatus=LinkStatus;
        
            if((LinkStatus & (kIONetworkLinkValid|kIONetworkLinkActive)) == (kIONetworkLinkValid|kIONetworkLinkActive)){
                //This is the best scenario.
                CFRelease(CFLinkStatus);
            } else if (LinkStatus & kIONetworkLinkActive) {
                //Having an invalid LinkStatus is not fatal. We just put an
                //interest notification and we get called to validate again if
                //something changes.
                log_debug("%s Validation Failed: Interface doesn't have a Valid Link but has an Active one", currentNetworkInterface->deviceName);
                IOServiceAddInterestNotification(notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                CFRelease(CFLinkStatus);
                return;
            } else if (LinkStatus & kIONetworkLinkValid) {
                //Having an invalid LinkStatus is not fatal. We just put an
                //interest notification and we get called to validate again if
                //something changes.
                log_debug("%s Validation Failed: Interface doesn't have an Active Link but has a Valid one", currentNetworkInterface->deviceName);
                IOServiceAddInterestNotification(notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                CFRelease(CFLinkStatus);
                return;
            }
        } else {
            //Not being able to read the LinkStatus (even if it's invalid) is fatal
            //and we ignore the interface from here on.
            log_err("%s Could not read linkStatus.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            free((void *)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        
        
        //
        // Get/Check the Medium Speed
        //
        CFTypeRef ioLinkSpeed = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkSpeed), kCFAllocatorDefault, 0);
        if (ioLinkSpeed) {
            CFNumberGetValue((CFNumberRef)ioLinkSpeed, kCFNumberLongLongType, &currentNetworkInterface->LinkSpeed);
            CFRelease(ioLinkSpeed);
        } else {
            //Not being able to read the LinkSpeed (even if it's ZERO) is fatal
            //and we ignore the interface from here on.
            log_err("%s Could not read ifLinkSpeed.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            free((void *)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        
        
        //
        //Get/Check the Medium Type
        //
        CFDictionaryRef properties = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMediumDictionary), kCFAllocatorDefault, kNilOptions);
        if (properties){
            CFNumberRef activeMediumIndex = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOActiveMedium), kCFAllocatorDefault, kNilOptions);
            if (activeMediumIndex!=NULL) {

                CFDictionaryRef activeMedium = (CFDictionaryRef)CFDictionaryGetValue(properties, activeMediumIndex);
                if (activeMedium!=NULL){
                    CFNumberGetValue(CFDictionaryGetValue(activeMedium, CFSTR(kIOMediumType)), kCFNumberLongLongType, &(currentNetworkInterface->MediumType));
//                    CFRelease(activeMedium); //It's released when we release properties
                }

                CFRelease(activeMediumIndex);
            }
            CFRelease(properties);
        } else {
            log_err("%s Could not read Active Medium Type.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            free((void*)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }

        
    } else {
        log_err("%s Could not get the interface controller", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    IOObjectRelease(IONetworkController);

    
    //
    // Get the interface MTU.
    //
    CFTypeRef ioInterfaceMTU = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOMaxTransferUnit), kCFAllocatorDefault, 0);
    if (ioInterfaceMTU) {
        CFNumberGetValue((CFNumberRef)ioInterfaceMTU, kCFNumberLongType, &currentNetworkInterface->MTU);
        CFRelease(ioInterfaceMTU);
    } else {
        log_err("%s Could not read ifMaxTransferUnit", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }


    //
    // Check if we're UP, BROADCAST and not LOOPBACK.
    //
    CFNumberRef CFFlags = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceFlags), kCFAllocatorDefault, kNilOptions);
    if (CFFlags){
        int64_t         flags = 0;
        CFNumberGetValue(CFFlags, kCFNumberIntType, &flags);
        currentNetworkInterface->flags = flags;

        if( (!(currentNetworkInterface->flags & (IFF_UP | IFF_BROADCAST))) | (currentNetworkInterface->flags & IFF_LOOPBACK)){
            log_err("%s Failed the flags check", currentNetworkInterface->deviceName);
            IOServiceAddInterestNotification(notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
            IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
            IOObjectRelease(IONetworkInterface);
            CFRelease(CFFlags);
            return;
        } else CFRelease(CFFlags);
    } else {
        log_err("%s Could not read Interface Flags", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    //
    // Get the Interface Type.
    // A WiFi always conforms to 802.11 and Ethernet, so we check the 802.11
    // class first.
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
    kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
    if (kernel_return!=KERN_SUCCESS) {
        log_err("%s IOServiceAddInterestNofitication Interface error: 0x%08x.", currentNetworkInterface->deviceName, kernel_return);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    

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
        if (kernel_return!=KERN_SUCCESS){
            log_err("%s IOServiceAddInterestNofitication Controller error: 0x%08x.", currentNetworkInterface->deviceName, kernel_return);
            IOObjectRelease(IONetworkInterface);
            free((void*)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        IOObjectRelease(IONetworkController);
        
    } else {
        log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    

    
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
        log_err("%s Could not init pthread attributes: %d", currentNetworkInterface->deviceName, returnVal);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    returnVal = pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_DETACHED);
    if (returnVal!=noErr){
        log_err("%s Could not set pthread attributes: %d", currentNetworkInterface->deviceName, returnVal);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    int threadError = pthread_create(&posixThreadID, &threadAttributes, (void *)&lltdLoop, currentNetworkInterface);
    
    currentNetworkInterface->posixThreadID = posixThreadID;
    returnVal = pthread_attr_destroy(&threadAttributes);
    
    assert(!returnVal);
    
    if (threadError != KERN_SUCCESS) {
        log_err("%s Could not launch thread with error %d.", currentNetworkInterface->deviceName, threadError);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    

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
#ifdef DEBUG
    if (IOObjectConformsTo(service, kIONetworkInterfaceClass)){
        log_debug("Notification received: %s type: %x networkInterfaceClass: %s", currentNetworkInterface->deviceName, messageType, nameString);
    } else if (IOObjectConformsTo(service, kIONetworkControllerClass)){
        log_debug("Notification received: %s type: %x networkControllerClass: %s", currentNetworkInterface->deviceName, messageType, nameString);
    } else {
        log_debug("Notification received: %s type: %x networkSomethingClass: %s", currentNetworkInterface->deviceName, messageType, nameString);
    }
#endif
    /*
     * Clean everything (thread, data, etc.)
     */
    if (messageType == kIOMessageServiceIsTerminated) {
        log_debug("Interface removed: %s", currentNetworkInterface->deviceName);
        pthread_cancel(currentNetworkInterface->posixThreadID);
        free((void *)currentNetworkInterface->deviceName);
        IOObjectRelease(currentNetworkInterface->notification);
        if(currentNetworkInterface->recvBuffer) free(currentNetworkInterface->recvBuffer);
        //Clean the linked list
        probe_t *currentProbe = currentNetworkInterface->seeList;
        void    *nextProbe;
        if (currentProbe){
            for(uint32_t i=0; i < currentNetworkInterface->seeListCount; i++){
                nextProbe     = currentProbe->nextProbe;
                free(currentProbe);
                currentProbe  = nextProbe;
            }
        }
        currentNetworkInterface->seeList = NULL;
        if (currentNetworkInterface->icon) free(currentNetworkInterface->icon);
        free(currentNetworkInterface);
    /*
     * Keep the struct, the notification and the deviceName.
     * The notification is there to let us know when the device comes back
     * to life.
     * The device name is for logging only.
     */
    } else if (messageType == kIOMessageDeviceWillPowerOff) {
        log_debug("Interface powered off: %s", currentNetworkInterface->deviceName);
        if(currentNetworkInterface->recvBuffer) {
            free(currentNetworkInterface->recvBuffer);
            currentNetworkInterface->recvBuffer = NULL;
        }
        //Clean the linked list
        probe_t *currentProbe = currentNetworkInterface->seeList;
        void    *nextProbe;
        if (currentProbe){
            for(uint32_t i = 0; i<currentNetworkInterface->seeListCount; i++){
                nextProbe     = currentProbe->nextProbe;
                free(currentProbe);
                currentProbe  = nextProbe;
            }
        }
        currentNetworkInterface->seeList = NULL;
        if (currentNetworkInterface->icon) free(currentNetworkInterface->icon);
        pthread_cancel(currentNetworkInterface->posixThreadID);
    /*
     * Restart like new.
     * I initially wanted to compare the data and correct the deltas,
     * but it's easier to just let it stabilize for 100miliseconds and try
     * one more time from scratch.
     */
    } else if (messageType == kIOMessageServicePropertyChange) {
        //We should have some cleanup over here. There are only two properties that we care about.
        log_debug("A property has changed. Reloading thread. %s", currentNetworkInterface->deviceName);
        pthread_cancel(currentNetworkInterface->posixThreadID);
        usleep(100000);
        if(currentNetworkInterface->recvBuffer) {
            free(currentNetworkInterface->recvBuffer);
            currentNetworkInterface->recvBuffer=NULL;
        }
        //Kill the notification. We'll get a new one on validateInterface.
        IOObjectRelease(currentNetworkInterface->notification);
        //Clean the linked list
        probe_t *currentProbe = currentNetworkInterface->seeList;
        void    *nextProbe;
        if (currentProbe){
            for(uint32_t i = 0; i<currentNetworkInterface->seeListCount; i++){
                nextProbe     = currentProbe->nextProbe;
                free(currentProbe);
                currentProbe  = nextProbe;
            }
        }
        currentNetworkInterface->seeList = NULL;
        if (currentNetworkInterface->icon) free(currentNetworkInterface->icon);
        close(currentNetworkInterface->socket);
        validateInterface(currentNetworkInterface, service);
    /*
     * Restart. Since everything except for the device name and notification was
     * removed on DeviceWillPowerOff, we are now removing also the notification and
     * device name.
     */
    } else if (messageType == kIOMessageDeviceWillPowerOn) {
        log_debug("Interface powered on: %s", currentNetworkInterface->deviceName);
        free((void *)currentNetworkInterface->deviceName);
        IOObjectRelease(currentNetworkInterface->notification);
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
        memset(currentNetworkInterface, 0, sizeof(network_interface_t));
        
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
        
        currentNetworkInterface->deviceName=malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(deviceName), kCFStringEncodingUTF8));
        CFStringGetCString(deviceName, (char *)currentNetworkInterface->deviceName, CFStringGetMaximumSizeForEncoding(CFStringGetLength(deviceName), kCFStringEncodingUTF8), kCFStringEncodingUTF8);
        CFRelease(deviceName);

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
    if (handler == SIG_ERR) log_crit("Could not establish SIGINT handler.");
    handler = signal(SIGTERM, SignalHandler);
    if (handler == SIG_ERR) log_crit("Could not establish SIGTERM handler.");
    
    
    //
    // Creates and returns a notification object for receiving
    // IOKit notifications of new devices or state changes
    //
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
    CFRunLoopRun();
    
    //
    // We should never get here
    //
    log_crit("Unexpectedly back from CFRunLoopRun()!");
    if (masterPort != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), masterPort);
    asl_close(asl);
    return EXIT_FAILURE;
    
}
