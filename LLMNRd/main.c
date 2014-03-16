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


void SignalHandler(int sigraised)
{
    fprintf(stderr, "\nInterrupted.\n");
    exit(0);
}

typedef struct networkInterface {
    io_object_t     notification;
    CFStringRef     deviceName;
    uint8_t         MACAddress [ kIOEthernetAddressSize ];
    uint8_t         InterfaceType;
    
    
    
} networkInterface;

IONotificationPortRef notificationPort;
CFRunLoopRef          runLoop;


void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument){
    
    kern_return_t       kernel_return;
    networkInterface   *currentNetworkInterface = (networkInterface*) refCon;
    
    if (messageType ==kIOMessageServiceIsTerminated) {
        printf("%s has been removed\n", CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8));
        
        CFRelease(currentNetworkInterface->deviceName);
        
        kernel_return = IOObjectRelease(currentNetworkInterface->notification);
        
        free(currentNetworkInterface);
    }
}

void deviceAppeared(void *refCon, io_iterator_t iterator){
    kern_return_t      kernel_return;
    io_service_t       IONetworkInterface;
    io_service_t       IONetworkController;
    

    
    while ((IONetworkInterface = IOIteratorNext(iterator))){
        io_name_t         ifname;
        networkInterface *currentNetworkInterface = NULL;
        
        //Let's initialize the networkInterface Data Structure
        currentNetworkInterface = malloc(sizeof(networkInterface));
        bzero(currentNetworkInterface,sizeof(networkInterface));
        
        kernel_return = IORegistryEntryGetName(IONetworkInterface, ifname);
        
        //Let's get the device name
        currentNetworkInterface->deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        
        //Let's let it stabilize. If we don't have a BSD name, it's not there
        if (currentNetworkInterface->deviceName == NULL) {
            usleep(50000);
            currentNetworkInterface->deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        }
        
        //Let's get the controller parent in order to get the Mac Address
        kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
        
        if (kernel_return != KERN_SUCCESS) printf("Could not get the parent of the interface\n");
        
        if(IONetworkController) {
            
            CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
            
            const CFDataRef refData = (const CFDataRef)macAddressAsData;
            
            CFDataGetBytes(refData, CFRangeMake(0,CFDataGetLength(refData)), currentNetworkInterface->MACAddress);
            CFRelease(macAddressAsData);
            
        }
        
        printf("Found interface: %s  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", CFStringGetCStringPtr(currentNetworkInterface->deviceName, kCFStringEncodingUTF8),currentNetworkInterface->MACAddress[0], currentNetworkInterface->MACAddress[1], currentNetworkInterface->MACAddress[2], currentNetworkInterface->MACAddress[3], currentNetworkInterface->MACAddress[4], currentNetworkInterface->MACAddress[5]);
        
        kernel_return = IOServiceAddInterestNotification(notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
        
        if (kernel_return!=KERN_SUCCESS) printf("IOServiceAddInterestNofitication error: 0x%08x.\n", kernel_return);
        
        kernel_return = IOObjectRelease(IONetworkInterface);
        kernel_return = IOObjectRelease(IONetworkController);

    }
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


    // Creates and returns a notification object for receiving IOKit
    // notifications of new devices or state changes
    masterPort = MACH_PORT_NULL;
    kernel_return = IOMasterPort(bootstrap_port, &masterPort);
    if (kernel_return != KERN_SUCCESS) {
        printf("IOMasterPort returned 0x%x\n", kernel_return);
        return 0;
    }
    
    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) printf("Could not establish new signal handler.\n");
    
    
    //Let's try to get device notifications
    newDevicesIterator = 0;
    notificationPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(notificationPort);

    runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopDefaultMode);


    kernel_return = IOServiceAddMatchingNotification(notificationPort, kIOFirstMatchNotification, IOServiceMatching(kIONetworkInterfaceClass), deviceAppeared, NULL, &newDevicesIterator);
    if (kernel_return!=KERN_SUCCESS) printf("IOServiceAddMatchingNotification(deviceAppeared) returned 0x%x\n", kernel_return);
    
    //Do an initial device scan
    deviceAppeared(NULL, newDevicesIterator);

    // Start the run loop.
    // As soon as I implement them, we'll receive notifications.
    printf("Starting run loop.\n\n");
    CFRunLoopRun();
    
    // We should never get here
    printf("Unexpectedly back from CFRunLoopRun()!\n");
    if (masterPort != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), masterPort);
    return 0;
    
}
