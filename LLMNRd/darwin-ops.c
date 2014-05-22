/******************************************************************************
 *                                                                            *
 *   darwin-ops.c                                                             *
 *   LLMNRd                                                                   *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 24.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/
#include "llmnrd.h"
#define debug 1

#pragma mark Functions that return machine information
#pragma mark -


#pragma mark Complete
//==============================================================================
//
// Returned in UUID binary format
//
//==============================================================================
void getUpnpUuid(void **pointer){
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                              IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFStringRef uuidStr = IORegistryEntryCreateCFProperty(platformExpert,
                                                              CFSTR(kIOPlatformUUIDKey),
                                                              kCFAllocatorDefault, 0);
        
        
        IOObjectRelease(platformExpert);

        CFUUIDRef cfUUID = CFUUIDCreateFromString(kCFAllocatorDefault, uuidStr);
        CFUUIDBytes uuidBytes = CFUUIDGetUUIDBytes(cfUUID);
        asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: %s %x\n", __FUNCTION__, CFStringGetCStringPtr(uuidStr, 0), uuidBytes.byte0);
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
    
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                              IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFDataRef modelCodeData = IORegistryEntryCreateCFProperty(platformExpert,
                                                                  CFSTR("model"),
                                                                  kCFAllocatorDefault, kNilOptions);
        //
        // FIXME: Doesn't get released.
        //
        (void) IOObjectRelease(platformExpert);
        
        CFStringRef modelCode = CFStringCreateWithCString(NULL, (char*)CFDataGetBytePtr(modelCodeData), kCFStringEncodingASCII);
        CFStringRef UniformTypeIdentifier = NULL;
        
        if (modelCode != NULL) {
            if(!(CFStringGetLength(modelCode)==0)){
                UniformTypeIdentifier = UTTypeCreatePreferredIdentifierForTag(CFSTR("com.apple.device-model-code"), modelCode, NULL);
            } else UniformTypeIdentifier = CFSTR("com.apple.mac");

            CFRelease(modelCode);
        } else {
            CFRelease(modelCodeData);
            return;
        }
        
        // Use to change the represented icon to another one
        // if you want to test the looks of another system
        // Uncomment to get an iphone-4 icon
        //UniformTypeIdentifier = CFSTR("com.apple.iphone-4");
        
        CFRelease(modelCodeData);

        
        CFDictionaryRef utiDeclaration = UTTypeCopyDeclaration(UniformTypeIdentifier);
        
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
            //FIXME: Doesn't get released
            CFRelease(CTBundle);
//            CFRelease(iconFileNameRef);//Gets released by releasing utiDeclaration
            
            //FIXME: Doesn't get released
            CFRelease(utiDeclaration);
            //FIXME: Doesn't get released
            CFRelease(UniformTypeIdentifier);

            
//            asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Icon URL: %s\n", __FUNCTION__, CFStringGetCStringPtr(CFURLGetString(iconURL), kCFStringEncodingUTF8));

            // Load the icns file
            CGImageSourceRef myIcon = CGImageSourceCreateWithURL(iconURL, NULL);
                
            CFRelease(iconURL);
                
            if( myIcon != NULL){
                    size_t iconCount = CGImageSourceGetCount (myIcon);

//                    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: icon count: %zu\n", __FUNCTION__, iconCount);

                    Boolean haveIconSelected = FALSE;
                    size_t iconSelected = 0;
                    long iconSelectedWidth = 0;
                    long iconSelectedHeight = 0;
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

//                            asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Icon index(%d): %4dx%4d %dbpp (%03dx%03d DPI) %dx%d (%d)\n", __FUNCTION__, index, (int)iconWidth, (int)iconHeight, (int)iconBpp, (int)iconDpiWidth, (int)iconDpiHeight, (int)iconSelectedHeight, (int)iconSelectedWidth, (int)iconSelected);
                            
                            // FIXME: Doesn't get released
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
                        CFStringRef keys[2];
                        CFTypeRef values[2];
                        uint32 iconSize = 48;
                        CFNumberRef thumbSizeRef = CFNumberCreate(NULL, kCFNumberIntType, &iconSize);
                        keys[0] = kCGImageSourceCreateThumbnailFromImageIfAbsent;
                        values[0] = (CFTypeRef)kCFBooleanTrue;
                        keys[1] = kCGImageSourceThumbnailMaxPixelSize;
                        values[1] = (CFTypeRef)thumbSizeRef;
                        options = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                        CFRelease(thumbSizeRef);
                        
                        // Create the thumbnail image using the options dictionary
                        CGImageRef thumbnailImageRef = CGImageSourceCreateThumbnailAtIndex(myIcon, iconSelected, (CFDictionaryRef)options);
                        
                        // Save the thumbnail image in a MutableDataRef (a very
                        // fancy Core Foundation buffer)
                        CFMutableDataRef myImageStream = CFDataCreateMutable(kCFAllocatorDefault, 0);
                        CGImageDestinationRef myImage = CGImageDestinationCreateWithData(myImageStream, kUTTypePNG, 1, NULL);
                        CGImageDestinationAddImage(myImage, thumbnailImageRef, NULL);
                        //FIXME: Doesn't get released
                        CFRelease(thumbnailImageRef);
                        CGImageDestinationFinalize(myImage);
                        //FIXME: Doesn't get released
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
                
                    //FIXME: Doesn't get released
                    CFRelease(myIcon);
                }

        } else {

            CFRelease(UniformTypeIdentifier);
        }

    }
}



//==============================================================================
//
// Returned in UCS-2LE
//
//==============================================================================
void getMachineName(char **pointer, size_t *stringSize){
    
    CFStringRef LocalHostName = SCDynamicStoreCopyLocalHostName(NULL);
    *stringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(LocalHostName), kCFStringEncodingUTF16LE);
    char *data = malloc(*stringSize);
    CFStringGetCString(LocalHostName, data, (CFIndex)stringSize, kCFStringEncodingUTF16LE);
    CFRelease(LocalHostName);
    *pointer = data;
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
    char *data = malloc(*stringSize);
    CFStringGetCString(FriendlyName, data, (CFIndex)stringSize, kCFStringEncodingUTF16LE);
    CFRelease(FriendlyName);
    *pointer = data;
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
            //
            //If the serial number is in the classic 11 digit format pe get
            //the last 3 digits
            //
            if(CFStringGetLength(serialNumber)==11) {
//                asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: 11 character serial number %s\n", __FUNCTION__, CFStringGetCStringPtr(serialNumber, 0));
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
                //
                //If it's in the modern 12 digit format, we get the last 4 digits.
                //
            } else if (CFStringGetLength(serialNumber)==12){
                asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: 12 character serial number %s\n", __FUNCTION__, CFStringGetCStringPtr(serialNumber, 0));
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
                //
                //If it's not 11 or 12 digits, we have no idea.
                //
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
void getDetailedIconImage(void *data);


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
void getComponentTable(void *data);


//==============================================================================
//
// Switches the interface in the argument to promscious mode
//
//==============================================================================
//==============================================================================
//
// Switches the interface in the argument to promscious mode
//
//==============================================================================
//==============================================================================
//
// Switches the interface in the argument to promscious mode
//
//==============================================================================
void setPromiscuous(void *networkInterface, boolean_t set){
    network_interface_t *currentNetworkInterface = (network_interface_t *) networkInterface;
    uint16_t       flags;
    
    CFNumberGetValue(currentNetworkInterface->flags, kCFNumberIntType, &flags);
    if ( ( flags & IFF_UP ) && ( flags & IFF_RUNNING ) ){
        struct ifreq IfRequest;
        
        bzero(&IfRequest, sizeof(IfRequest));
        (void) CFStringGetCString(currentNetworkInterface->deviceName, IfRequest.ifr_name, sizeof(IfRequest.ifr_name), kCFStringEncodingASCII);
        
        if (ioctl(currentNetworkInterface->socket, SIOCGIFFLAGS, (caddr_t)&IfRequest) == -1) {
            asl_log(asl,log_msg, ASL_LEVEL_ERR, "%s: could not get flags for interface \"%s\": %s\n", __FUNCTION__, IfRequest.ifr_name, strerror(errno));
            return FALSE;
        }
        
        if (set) {
            IfRequest.ifr_flags |= IFF_PROMISC;
        } else {
            IfRequest.ifr_flags &= ~IFF_PROMISC;
        }
        
        if (ioctl(currentNetworkInterface->socket, SIOCSIFFLAGS, (caddr_t)&IfRequest) == -1) {
            asl_log(asl,log_msg, ASL_LEVEL_ERR, "%s: could not set flags for interface \"%s\": %s\n", __FUNCTION__, IfRequest.ifr_name, strerror(errno));
            return FALSE;
        }
        
        return TRUE;
    }
    
}


