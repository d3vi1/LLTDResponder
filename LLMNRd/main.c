#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>


//================================================================================================
//
//	SignalHandler
//
//	This routine will get called when we interrupt the program (usually with a Ctrl-C from the
//	command line).
//
//================================================================================================
void SignalHandler(int sigraised)
{
    fprintf(stderr, "\nInterrupted.\n");
    
    exit(0);
}

//================================================================================================
//	main
//================================================================================================
int main(int argc, const char *argv[])
{
    io_iterator_t   iterator;
    io_service_t    interface;
    io_service_t    controller;
    IOReturn        kernel_return;
    CFStringRef     string; //used all over the place
    CFNumberRef     number; //used all over the place
    
    //GetAllNetworkInterfaces
    kernel_return = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                                 IOServiceMatching(kIONetworkInterfaceClass),
                                                 &iterator);
    
    if(kernel_return == 0) {
        //Loop through the network interfaces
        while ((interface = IOIteratorNext(iterator))) {
            
            kernel_return = IORegistryEntryGetParentEntry( interface, kIOServicePlane, &controller);
            
            if(controller) {
                CFStringRef name = IORegistryEntryCreateCFProperty(interface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
                CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(controller, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
                const CFDataRef refData = (const CFDataRef)macAddressAsData;
                UInt8 MACAddress[ kIOEthernetAddressSize ];
                
                CFDataGetBytes(refData, CFRangeMake(0,CFDataGetLength(refData)), MACAddress);
                CFRelease(macAddressAsData);
                
                if(name && CFGetTypeID(name) == CFStringGetTypeID()) {
                    //We get the interface name as CFString and the macAddress as CFData
                    printf("Found interface: %s  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", CFStringGetCStringPtr(name, kCFStringEncodingUTF8),MACAddress[0], MACAddress[1], MACAddress[2], MACAddress[3], MACAddress[4], MACAddress[5]);
                }
                string = IORegistryEntryCreateCFProperty(controller, CFSTR(kIOActiveMedium), kCFAllocatorDefault, 0);
                
                printf("Active  medium : %s\n", CFStringGetCStringPtr(string, kCFStringEncodingUTF8));
                
                number = IORegistryEntryCreateCFProperty(controller, CFSTR(kIOLinkStatus), kCFAllocatorDefault, 0);
                if (number) {
                    long status;
                    
                    if (CFNumberGetValue(number, kCFNumberLongType, &status))
                    {
                        printf("Link status    : ");
                        
                        if (status & kIONetworkLinkValid)
                        {
                            printf("%s\n", (status & kIONetworkLinkActive) ? "Active" :
                                   "Inactive");
                        }
                        else
                            printf("Not reported\n");
                    }
                }
            }
            IOObjectRelease(controller);
            
            IOObjectRelease(interface);
        }
        IOObjectRelease(iterator);
        
    }
    
    // Start the run loop. Now we'll receive notifications.
    fprintf(stderr, "Starting run loop.\n\n");
    //CFRunLoopRun();
    
    // We should never get here
    fprintf(stderr, "Unexpectedly back from CFRunLoopRun()!\n");
    return 0;
}
