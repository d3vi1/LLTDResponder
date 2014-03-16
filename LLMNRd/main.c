//
//  llmnrd.c
//  LLMNRd
//
//  Created by Răzvan Corneliu C.R. VILT on 16.03.2014.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//

#include "llmnrd.h"

//==============================================================================
// We are currently checking if it's an Ethernet Interface
//==============================================================================
void SignalHandler(int sigraised) {
    fprintf(stderr, "\nInterrupted.\n");
    exit(0);
}

//==============================================================================
// We are currently checking if it's an Ethernet Interface
//==============================================================================
boolean_t validateInterface(void *refCon) {
    networkInterface  *currentNetworkInterface = (networkInterface*) refCon;
    
    return true;
}

void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument){
    
    kern_return_t       kernel_return;
    networkInterface   *currentNetworkInterface = (networkInterface*) refCon;
    
    //
    // XXX: should we listen for kIOMessageServiceIsRequestingClose instead?!?
    //
    if (messageType ==kIOMessageServiceIsTerminated) {
        printf("%s has been removed\n", CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8));
        
        CFRelease(currentNetworkInterface->deviceName);
        
        kernel_return = IOObjectRelease(currentNetworkInterface->notification);
        
        free(currentNetworkInterface);
    }
}

//==============================================================================
// We identify the network interface and
//==============================================================================
void deviceAppeared(void *refCon, io_iterator_t iterator){

    kern_return_t      kernel_return;
    io_service_t       IONetworkInterface;
    io_service_t       IONetworkController;
    
    //
    // Let's loop through the interfaces that we've got
    //
    while ((IONetworkInterface = IOIteratorNext(iterator))){

        //
        // Let's initialize the networkInterface Data Structure
        //
        networkInterface *currentNetworkInterface = NULL;
        currentNetworkInterface = malloc(sizeof(networkInterface));
        bzero(currentNetworkInterface,sizeof(networkInterface));
        
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
        
        //
        // Get the object parent (the interface controller) in order to get the
        // Mac Address from it.
        //
        kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
        
        if (kernel_return != KERN_SUCCESS) printf("Could not get the parent of the interface\n");
        
        if(IONetworkController) {
            
            CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
            
            const CFDataRef refData = (const CFDataRef)macAddressAsData;
            
            CFDataGetBytes(refData, CFRangeMake(0,CFDataGetLength(refData)), currentNetworkInterface->MACAddress);
            CFRelease(macAddressAsData);
            
        }
        
        //
        // DEBUG: Print the network interfaces to stdout
        //
        printf("Found interface: %s  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8),currentNetworkInterface->MACAddress[0], currentNetworkInterface->MACAddress[1], currentNetworkInterface->MACAddress[2], currentNetworkInterface->MACAddress[3], currentNetworkInterface->MACAddress[4], currentNetworkInterface->MACAddress[5]);
        
        
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
        kernel_return = IOObjectRelease(IONetworkInterface);
        kernel_return = IOObjectRelease(IONetworkController);

    } // while ((IONetworkInterface = IOIteratorNext(iterator))
}


//================================================================================================
//	main
//================================================================================================
int main(int argc, const char *argv[])
{
    sig_t                 handler;
    kern_return_t         kernel_return;
    mach_port_t           masterPort;
    CFRunLoopSourceRef    runLoopSource;
    io_iterator_t         newDevicesIterator;


    //
    // Set up a signal handler so we can clean up when we're interrupted from
    // the command line. Otherwise we stay in our run loop forever.
    //
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) printf("Could not establish new signal handler.\n");
    
    
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
