/******************************************************************************
 *                                                                            *
 *   darwin-ops.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 24.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/

#include "lltdDaemon.h"

#pragma mark Functions that return machine information
#pragma mark -


#pragma mark Complete
//==============================================================================
//
// Returned in UUID binary format
//
//==============================================================================
void getUpnpUuid(void **pointer){
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    
    if (platformExpert) {
        CFStringRef uuidStr = IORegistryEntryCreateCFProperty(platformExpert,
                                                              CFSTR(kIOPlatformUUIDKey),
                                                              kCFAllocatorDefault, 0);
        
        
        IOObjectRelease(platformExpert);

        CFUUIDRef cfUUID = CFUUIDCreateFromString(kCFAllocatorDefault, uuidStr);
        CFUUIDBytes uuidBytes = CFUUIDGetUUIDBytes(cfUUID);
        *pointer = malloc(sizeof(uuidBytes));
        memcpy(*pointer, &uuidBytes, sizeof(CFUUIDBytes));
        CFRelease(cfUUID);
        CFRelease(uuidStr);
        return;
        
    } else pointer = NULL;
}



//==============================================================================
//
// Returns a copy of the icon image in Microsoft ICO format.
// Made from an image larger or equal to 48px in size.
// The icon must be smaller than 32kb which we are not yet verifying.
// Only with QueryLargetTLV
//
//==============================================================================
void getIconImage(void **icon, size_t *iconsize){
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));

    if (platformExpert) {
        CFDataRef modelCodeData = IORegistryEntryCreateCFProperty(platformExpert,
                                                                  CFSTR("model"),
                                                                  kCFAllocatorDefault, kNilOptions);
        //
        // FIXME: Doesn't get released.
        //
        (void) IOObjectRelease(platformExpert);
        
        CFStringRef modelCode = CFStringCreateWithCString(kCFAllocatorDefault, (char*)CFDataGetBytePtr(modelCodeData), kCFStringEncodingASCII);
        CFStringRef UniformTypeIdentifier = NULL;
        
        if (modelCode != NULL) {
            if((long) CFStringGetLength(modelCode) > 0) {
                UniformTypeIdentifier = UTTypeCreatePreferredIdentifierForTag(CFSTR("com.apple.device-model-code"), modelCode, NULL);
            } else {
                UniformTypeIdentifier = CFSTR("com.apple.mac");
            }

            CFRelease(modelCode);
        } else {
            CFRelease(modelCodeData);
            return;
        }

        if (CFStringHasPrefix(UniformTypeIdentifier, CFSTR("dyn"))) {
            UniformTypeIdentifier = CFSTR("com.apple.mac");
        }
        
        CFRelease(modelCodeData);

        CFDictionaryRef utiDeclaration = UTTypeCopyDeclaration(UniformTypeIdentifier);
        
        CFRelease(UniformTypeIdentifier);

        if (utiDeclaration){
            
            CFStringRef iconFileNameRef = CFDictionaryGetValue(utiDeclaration, CFSTR("UTTypeIconFile"));

            // if there is no icon for this UTI, load the icon for the key conforms to UTI.
            // cycle until we find one, there is always one at the top of the tree.
            // && (nil != utiDeclaration))
            if (iconFileNameRef){
                while (nil==iconFileNameRef){
                    UniformTypeIdentifier = CFDictionaryGetValue(utiDeclaration, CFSTR("UTTypeConformsTo"));
                    utiDeclaration = UTTypeCopyDeclaration(UniformTypeIdentifier);
                    iconFileNameRef = CFDictionaryGetValue(utiDeclaration, CFSTR("UTTypeIconFile"));
                }
            }
            CFURLRef CTBundleUrlRef = UTTypeCopyDeclaringBundleURL(UniformTypeIdentifier);
            CFBundleRef CTBundle = CFBundleCreate(kCFAllocatorDefault, CTBundleUrlRef);

            CFURLRef iconURL = CFBundleCopyResourceURL(CTBundle, iconFileNameRef, NULL, NULL);
            CFRelease(CTBundleUrlRef);
            CFRelease(CTBundle);
            CFRelease(utiDeclaration);

            // Load the icns file
            CGImageSourceRef myIcon = CGImageSourceCreateWithURL(iconURL, NULL);
                
            CFRelease(iconURL);
                
            if( myIcon != NULL){
                    size_t  iconCount          = CGImageSourceGetCount (myIcon);
                    Boolean haveIconSelected   = FALSE;
                    size_t  iconSelected       = 0;
                    long    iconSelectedWidth  = 0;
                    long    iconSelectedHeight = 0;
                
                    if(iconCount > 0) {
                        
                        //
                        // Go through all the icons to get their properties
                        //
                        
                        for(int index=0; index<iconCount; index++){
                            
                            CFDictionaryRef imageProperties = NULL;
                            imageProperties = CGImageSourceCopyPropertiesAtIndex(myIcon, index, NULL);
                            
                            long iconWidth = 0;
                            long iconHeight = 0;
                            long iconBpp = 0;
                            long iconDpiWidth = 0;
                            long iconDpiHeight = 0;
                            
                            CFNumberGetValue(CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelWidth), kCFNumberLongType, &iconWidth);
                            CFNumberGetValue(CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelHeight), kCFNumberLongType, &iconHeight);
                            CFNumberGetValue(CFDictionaryGetValue(imageProperties, kCGImagePropertyDPIWidth), kCFNumberLongType, &iconDpiWidth);
                            CFNumberGetValue(CFDictionaryGetValue(imageProperties, kCGImagePropertyDPIHeight), kCFNumberLongType, &iconDpiHeight);
                            CFNumberGetValue(CFDictionaryGetValue(imageProperties, kCGImagePropertyDepth), kCFNumberLongType, &iconBpp);
                            
                            //
                            // Get the index of the smallest icon that is above or equal to 48x48
                            // FIXME: If we have multiple icons of the same size choose the one with 72 dpi or the highest bit depth.
                            //
                            if (((iconSelectedWidth >= iconWidth)&(iconWidth >= 48)) | (haveIconSelected == FALSE)){
                                iconSelectedHeight = iconHeight;
                                iconSelectedWidth = iconWidth;
                                iconSelected = index;
                                haveIconSelected = TRUE;
                            }

                            log_debug("Icon index(%d): %4dx%4d %dbpp (%03dx%03d DPI) %dx%d (%d)", index, (int)iconWidth, (int)iconHeight, (int)iconBpp, (int)iconDpiWidth, (int)iconDpiHeight, (int)iconSelectedHeight, (int)iconSelectedWidth, (int)iconSelected);
                            
                            CFRelease(imageProperties);
                        }
                        
                        
                        
                        //
                        // Great idea to use thumbnail generation from ImageIO
                        // Taken from:
                        // http://cocoaintheshell.com/2011/01/uiimage-scaling-imageio/
                        // We start first by defining thumbnail characteristics as
                        // 48 px in an options dictionary. It's difficult to resist
                        // the tempation to use Foundation/UIKit/Appkit APIs and
                        // find alternatives. Google wasn't much of a friend here.
                        //
                        CFDictionaryRef options = NULL;
                        CFStringRef     keys[2];
                        CFTypeRef       values[2];
                        uint32          iconSize = 48;
                        CFNumberRef     thumbSizeRef = CFNumberCreate(NULL, kCFNumberIntType, &iconSize);
                        keys[0]   = kCGImageSourceCreateThumbnailFromImageIfAbsent;
                        values[0] = (CFTypeRef)kCFBooleanTrue;
                        keys[1]   = kCGImageSourceThumbnailMaxPixelSize;
                        values[1] = (CFTypeRef)thumbSizeRef;
                        options   = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                        
                        // Create the thumbnail image using the options dictionary
                        CGImageRef thumbnailImageRef = CGImageSourceCreateThumbnailAtIndex(myIcon, iconSelected, (CFDictionaryRef)options);
                        
                        // Save the thumbnail image in a MutableDataRef (a very
                        // fancy Core Foundation buffer)
                        CFMutableDataRef myImageStream = CFDataCreateMutable(kCFAllocatorDefault, 0);
                        CGImageDestinationRef myImage = CGImageDestinationCreateWithData(myImageStream, kUTTypePNG, 1, NULL);
                        CGImageDestinationAddImage(myImage, thumbnailImageRef, NULL);
                        CFRelease(thumbnailImageRef);
                        CFRelease(thumbSizeRef);
                        CGImageDestinationFinalize(myImage);
                        CFRelease(myImage);
                        
                        // Now let's make an ICO out of that PNG. We have:
                        // * one iconDir
                        // * one iconDirEntry (for the single icon)
                        // * one PNG file of 48x48
                        // * the icon file structs are always little endian so we use
                        //   the CFSwapHostToLittle to swap if running on big-endian
                        //   architectures to little-endian
                        iconDir *myICOFile;
                        myICOFile = malloc(CFDataGetLength(myImageStream) + sizeof(iconDir));
                        myICOFile->idReserved             =  0;
                        myICOFile->idType                 = CFSwapInt16HostToLittle(icoDirTypeIcon);
                        myICOFile->idCount                = CFSwapInt16HostToLittle(1);
                        myICOFile->idEntries->Width       = CFSwapInt16HostToLittle(48);
                        myICOFile->idEntries->Height      = CFSwapInt16HostToLittle(48);
                        myICOFile->idEntries->ColorCount  =  0;
                        myICOFile->idEntries->Reserved    =  0;
                        myICOFile->idEntries->Planes      = CFSwapInt16HostToLittle(1);
                        myICOFile->idEntries->BitCount    = CFSwapInt16HostToLittle(32);
                        myICOFile->idEntries->BytesInRes  = CFSwapInt32HostToLittle((UInt32)CFDataGetLength(myImageStream));
                        myICOFile->idEntries->ImageOffset = CFSwapInt32HostToLittle((UInt32)sizeof(iconDir));
                        CFDataGetBytes(myImageStream,  CFRangeMake(0,CFDataGetLength(myImageStream)), (void *)myICOFile + myICOFile->idEntries->ImageOffset);
                        
                        // Set the return values
                        *icon = myICOFile;
                        *iconsize = (CFDataGetLength(myImageStream) + sizeof(iconDir));
                        CFRelease(myImageStream);
                    }
                
                    CFRelease(myIcon);
                }

        } else {
            CFRelease(UniformTypeIdentifier);
            IOObjectRelease(platformExpert);
        }
    }
}



//==============================================================================
//
// Returned in UCS-2LE
//
//==============================================================================
void getMachineName(char **pointer, size_t *stringSize){
    //FIXME: also breaks on subsequent scans
    CFStringRef LocalHostName = SCDynamicStoreCopyLocalHostName(NULL);
    *stringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(LocalHostName), kCFStringEncodingUTF16LE);
    char *data  = malloc(*stringSize);
    CFStringGetCString(LocalHostName, data, (CFIndex)stringSize, kCFStringEncodingUTF16LE);
    CFRelease(LocalHostName);
    *pointer    = data;
}



//==============================================================================
//
// Returned in UCS-2LE only with QueryLargeTLV
//
//==============================================================================
void getFriendlyName(char **pointer, size_t *stringSize){
    CFStringEncoding UCS2LE = kCFStringEncodingUTF16LE;
    CFStringRef FriendlyName = SCDynamicStoreCopyComputerName(NULL, &UCS2LE);
    *stringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(FriendlyName), kCFStringEncodingUTF16LE);
    char *data  = malloc(*stringSize);
    CFStringGetCString(FriendlyName, data, (CFIndex)stringSize, kCFStringEncodingUTF16LE);
    CFRelease(FriendlyName);
    *pointer    = data;
}



//==============================================================================
//
// Returned in UCS-2LE
// Returns the following URL with the last 3 or 4 digits of the serial number
// at the CC argument http://support-sp.apple.com/sp/index?page=psp&cc=XXX
// Taken from:
// /Applications/Utilities/System Profiler.app/Contents/Resources/SupportLinks.strings
//
//==============================================================================
void getSupportInfo(void **data, size_t *stringSize){
    //
    // Get the serial number from the IOPlatformExpertDevice
    //
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    
    if (platformExpert) {

        CFStringRef serialNumber = IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);
        
        IOObjectRelease(platformExpert);
        
        if (serialNumber != NULL) {
            //If the serial number is in the classic 11 digit format pe get
            //the last 3 digits
            if(CFStringGetLength(serialNumber)==11) {
                CFMutableStringRef URLString = CFStringCreateMutable(kCFAllocatorDefault, 0);
                if (URLString != NULL){
                    CFStringAppend(URLString, CFSTR("http://support-sp.apple.com/sp/index?page=psp&cc="));
                    CFStringRef last3Digits = CFStringCreateWithSubstring(kCFAllocatorDefault, serialNumber, CFRangeMake(8, 3));
                    CFStringAppend(URLString, last3Digits);
                    CFRelease(last3Digits);
                    *data = malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(URLString), kCFStringEncodingUTF8));
                    CFStringGetCString(URLString, *data, CFStringGetMaximumSizeForEncoding(CFStringGetLength(URLString), kCFStringEncodingUTF8),    kCFStringEncodingUTF8);
                    CFRelease(URLString);
                }
            //If it's in the modern 12 digit format, we get the last 4 digits.
            } else if (CFStringGetLength(serialNumber)==12){
                CFMutableStringRef URLString = CFStringCreateMutable(kCFAllocatorDefault, 0);
                if (URLString != NULL){
                    CFStringAppend(URLString, CFSTR("http://support-sp.apple.com/sp/index?page=psp&cc="));
                    CFStringRef last4Digits = CFStringCreateWithSubstring(kCFAllocatorDefault, serialNumber, CFRangeMake(8, 4));
                    CFStringAppend(URLString, last4Digits);
                    CFRelease(last4Digits);
                    *data=malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(URLString), kCFStringEncodingUTF8));
                    CFStringGetCString(URLString, *data, CFStringGetMaximumSizeForEncoding(CFStringGetLength(URLString), kCFStringEncodingUTF8), kCFStringEncodingUTF8);
                    CFRelease(URLString);
                }
            //Not 12 and not 11 digits, probably not a Mac
            } else {
                data = NULL;
            }
            
            CFRelease(serialNumber);
            return;
        }
        
    }
    
    data = NULL;
    return;
}



//==============================================================================
//
// Returns
// TRUE  = Ad-Hoc or disconnected
// FALSE = Infrastructure mode
//
//==============================================================================
boolean_t getWifiMode(void *networkInterface){
    network_interface_t *currentNetworkInterface = (network_interface_t*)networkInterface;
    
    struct ifmediareq IFMediaRequest;
    int   *MediaList, i;
    
    (void) memset(&IFMediaRequest, 0, sizeof(IFMediaRequest));
    (void) strncpy(IFMediaRequest.ifm_name, currentNetworkInterface->deviceName, sizeof(IFMediaRequest.ifm_name));
    
    if (ioctl(currentNetworkInterface->socket, SIOCGIFMEDIA, (caddr_t)&IFMediaRequest) < 0) {
        /*
         * Interface doesn't support SIOC{G,S}IFMEDIA.
         */
        return;
    }
    MediaList = (int *)malloc(IFMediaRequest.ifm_count * sizeof(int));
    if (MediaList == NULL) log_err("%s malloc error", currentNetworkInterface->deviceName);
    
    IFMediaRequest.ifm_ulist = MediaList;
    
    if (ioctl(currentNetworkInterface->socket, SIOCGIFMEDIA, (caddr_t)&IFMediaRequest) < 0)
        log_err("%s SIOCGIFMEDIA error", currentNetworkInterface->deviceName);
    
    if (IFMediaRequest.ifm_status & IFM_AVALID) {
        if(IFM_TYPE(IFMediaRequest.ifm_active)== IFM_IEEE80211){
            if(IFM_OPTIONS(IFMediaRequest.ifm_active)==IFM_IEEE80211_ADHOC){
                free(MediaList);
                return 0;
            } else {
                free(MediaList);
                return 1;
            }
        }
    }
    free(MediaList);
}



//==============================================================================
//
// Returns a copy of the 6 bytes comprising the BSSID.
//
//==============================================================================
boolean_t getBSSID (void **data, void *networkInterface){
    network_interface_t *currentNetworkInterface = (network_interface_t*)networkInterface;

    io_service_t  IONetworkInterface;
    kern_return_t kernel_return;
    mach_port_t   masterPort = MACH_PORT_NULL;
    kernel_return = IOMasterPort(bootstrap_port, &masterPort);
    if (kernel_return != KERN_SUCCESS) {
        log_err("IOMasterPort returned 0x%x", kernel_return);
        return EXIT_FAILURE;
    }
    
    IONetworkInterface = IOServiceGetMatchingService(masterPort, IOBSDNameMatching(masterPort, NULL, currentNetworkInterface->deviceName));
    
    if(IONetworkInterface){
        CFDataRef cfDATA = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR("IO80211BSSID"), kCFAllocatorDefault, 0);
        *data = malloc(kIOEthernetAddressSize);
        memcpy(*data,CFDataGetBytePtr(cfDATA),kIOEthernetAddressSize);
        CFRelease(cfDATA);
        IOObjectRelease(IONetworkInterface);
        return true;
    } else {
        data = NULL;
        return false;
    }
    
}



#pragma mark -
#pragma mark Almost complete/with minor bugs

#pragma mark -
#pragma mark Half way there. Scheleton done.

#pragma mark -
#pragma mark Not yet written
//==============================================================================
//
// See 2.2.2.3 in UCS-2LE
//
//==============================================================================
void getHwId(void *data);


//==============================================================================
//
// Only with QueryLargeTLV
// Returns a copy of the icon image in Microsoft ICO format
// at the followin sizes: 16x16, 32x32, 64x64, 128x128, 256x256
// and if available, 24x24 and 48x48
//
//==============================================================================
void getDetailedIconImage (void **data, size_t *iconsize);;


//==============================================================================
//
// Hmm... This will take a bit of work
// I know that we are a host that is
// hub+switch
//
//==============================================================================
void getHostCharacteristics (void *data);


//==============================================================================
//
// Only with QueryLargeTLV
//
//==============================================================================
void getComponentTable (void **data, size_t *dataSize);


//==============================================================================
//
// FIXME: Switches the interface in the argument to promscuous mode
// FIXME: The flags need to be read from the interface not from the cache.
//        Other applications might change the flags although we should get
//        IOKit notifications about it.
//
//==============================================================================
void setPromiscuous(void *networkInterface, boolean_t set){
    network_interface_t *currentNetworkInterface = networkInterface;
    
    long           flags;
    
    uint8_t        ifNameLength = min(strlen(currentNetworkInterface->deviceName), 16);
    char          *interfaceName = malloc (ifNameLength);

    strncpy(interfaceName, currentNetworkInterface->deviceName, 16);
    
    log_debug("Trying to set PROMISCUOUS=%d on IF=%s", set, interfaceName);
    if ( ( currentNetworkInterface->flags & IFF_UP ) && ( currentNetworkInterface->flags & IFF_RUNNING ) ){
        struct ifreq IfRequest;
        memset(&IfRequest, 0, sizeof(IfRequest));
        
        memcpy(&(IfRequest.ifr_name), interfaceName, ifNameLength);

        if ((currentNetworkInterface->flags & IFF_PROMISC) && set) {
            currentNetworkInterface->flags = currentNetworkInterface->flags | IFF_PROMISC;
            goto cleanup;
        } else {
            currentNetworkInterface->flags = currentNetworkInterface->flags & (!IFF_PROMISC);
        }
        
        int ioctlError  = ioctl(currentNetworkInterface->socket, SIOCGIFFLAGS, (caddr_t)&IfRequest);
        if (ioctlError) {
            log_err("Could not get flags for interface: %s", strerror(ioctlError));
        }
        
        if (set) {
            IfRequest.ifr_flags |= IFF_PROMISC;
        } else {
            IfRequest.ifr_flags &= ~IFF_PROMISC;
        }
        
        if (ioctl(currentNetworkInterface->socket, SIOCSIFFLAGS, (caddr_t)&IfRequest) == -1) {
            log_notice("Could not set flags for interface \"%s\": %s", IfRequest.ifr_name, strerror(errno));
            goto cleanup;
        }
    }
    cleanup:
    free(interfaceName);
}

