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

#pragma mark Functions that return machine information
// Returned in UCS-2LE
// SCDynamicStoreCopyLocalHostName
void getMachineName(char **pointer, size_t *stringSize){
    CFStringRef LocalHostName = SCDynamicStoreCopyLocalHostName(NULL);
    // UCS-2 is two bytes per char
    // CFStringGetLength tells us the number of chars
    // CFStringGetMaximumSizeForEncoding tells us the number of bytes
    *stringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(LocalHostName), kCFStringEncodingUTF16LE);
    char *data = malloc(*stringSize);
    CFStringGetCString(LocalHostName, data, (CFIndex)stringSize, kCFStringEncodingUTF16LE);
    CFRelease(LocalHostName);
    *pointer = data;
}

// Returned in UCS-2LE only with QueryLargeTLV
// SCDynamicStoreCopyComputerName
void getFriendlyName(char **pointer, size_t *stringSize){
    CFStringEncoding UCS2LE = kCFStringEncodingUTF16LE;
    CFStringRef FriendlyName = SCDynamicStoreCopyComputerName(NULL, &UCS2LE);
    // UCS-2 is two bytes per char
    // CFStringGetLength tells us the number of chars
    // CFStringGetMaximumSizeForEncoding tells us the number of bytes
    *stringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(FriendlyName), kCFStringEncodingUTF16LE);
    char *data = malloc(*stringSize);
    CFStringGetCString(FriendlyName, data, (CFIndex)stringSize, kCFStringEncodingUTF16LE);
    CFRelease(FriendlyName);
    *pointer = data;
}

// Returned in UCS-2LE
void getSupportInfo(void *data){
    //http://support-sp.apple.com/sp/index?page=psp&cc=AGZ last 3 digits of SN
    //From: /A/Util/System Prof.app/Contents/Resources/SupportLinks.strings
    //URL_OSX_SUPPORT	= "http://support-sp.apple.com/sp/index?page=psp&osver=%@&lang=%@"
    
}
// See 2.2.2.3 in UCS-2LE
void getHwId(void *data);
// Only with QueryLargeTLV
void getDetailedIconImage(void *data);
// 
void getHostCharacteristics (void *data);
//Only with QueryLargeTLV
void getComponentTable(void *data);



// Returns a copy of the icon image
void getIconImage(void *icon, size_t *iconsize){

    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                              IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFDataRef modelCodeData = IORegistryEntryCreateCFProperty(platformExpert,
                                        CFSTR("model"),
                                        kCFAllocatorDefault, kNilOptions);
        //
        // FIXME: Doesn't get released.
        //
        IOObjectRelease(platformExpert);

        CFStringRef modelCode = CFStringCreateWithCString(NULL, (char*)CFDataGetBytePtr(modelCodeData), kCFStringEncodingASCII);
        CFStringRef UniformTypeIdentifier = NULL;

        if ((modelCode != NULL) && (!(CFStringGetLength(modelCode)==0))){
            UniformTypeIdentifier = UTTypeCreatePreferredIdentifierForTag(CFSTR("com.apple.device-model-code"), modelCode, NULL);
        }
        else{
            UniformTypeIdentifier = CFSTR("com.apple.mac");
        }
        
        // Use to change the represented icon
        //UniformTypeIdentifier = CFSTR("com.apple.iphone-4");
        CFRelease(modelCodeData);
        CFRelease(modelCode);

        CFDictionaryRef utiDeclaration = UTTypeCopyDeclaration(UniformTypeIdentifier);

        if (utiDeclaration){
            CFStringRef iconFileNameRef = CFDictionaryGetValue(utiDeclaration, CFSTR("UTTypeIconFile"));
            // if there is no icon for this UTI, load the icon for the key conformsTo UTI.
            // cycle until we find one, there is always one at the top of the tree.
            if (nil == iconFileNameRef){
                while ((nil==iconFileNameRef) && (nil != utiDeclaration)){
                    UniformTypeIdentifier = CFDictionaryGetValue(utiDeclaration, CFSTR("UTTypeConformsTo"));
                    utiDeclaration = UTTypeCopyDeclaration(UniformTypeIdentifier);
                    iconFileNameRef = CFDictionaryGetValue(utiDeclaration, CFSTR("UTTypeIconFile"));
                }
            }

            CFBundleRef CTBundle = CFBundleCreate(kCFAllocatorDefault, UTTypeCopyDeclaringBundleURL(UniformTypeIdentifier));
            CFURLRef iconURL = CFBundleCopyResourceURL(CTBundle, iconFileNameRef, NULL, NULL);

            CFRelease(CTBundle);
            CFRelease(iconFileNameRef);
            CFRelease(UniformTypeIdentifier);

#ifdef debug
            printf("Icon URL: %s\n", CFStringGetCStringPtr(CFURLGetString(iconURL), kCFStringEncodingUTF8));
#endif
            // Load the icns file
            CGImageSourceRef myIcon = CGImageSourceCreateWithURL(iconURL, NULL);
            
            CFRelease(iconURL);
            
            //
            // FIXME: can't release dictionaries, but they get released by the
            // compiler at the end of the function.
            //
            //CFRelease(utiDeclaration);
            
            if(myIcon!=NULL){
                size_t iconCount = CGImageSourceGetCount (myIcon);
#ifdef debug
                printf("icon count: %zu\n", iconCount);
#endif
                Boolean haveIconSelected = FALSE;
                size_t iconSelected = 0;
                long iconSelectedWidth = 0;
                long iconSelectedHeight = 0;
                if(iconCount > 0) {
                    
                    //
                    // Go through all the icons to get their properties
                    //
                    
                    for(int index=0; index<iconCount; index++){
                        
                        //We create a CFData stream (myImageStream) to have the image
                        //and we get the png in there
                        CFMutableDataRef myImageStream = CFDataCreateMutable(kCFAllocatorDefault, 0);
                        CGImageDestinationRef myImage = CGImageDestinationCreateWithData(myImageStream, kUTTypePNG, 1, NULL);
                        CGImageDestinationAddImageFromSource(myImage, myIcon, index, NULL);
                        CGImageDestinationFinalize(myImage);

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
                        // FIXME: If we have multiple icons of the same size choose the one with 72 dpi
                        //        or the highest bit depth.
                        //
                        if (((iconSelectedWidth >= iconWidth)&(iconWidth >= 48)) | (haveIconSelected == FALSE)){
                                iconSelectedHeight = iconHeight;
                                iconSelectedWidth = iconWidth;
                                iconSelected = index;
                                haveIconSelected = TRUE;
                        }
#ifdef debug

                        printf("Icon index(%d): %4dx%4d %dbpp (%03dx%03d DPI)(%d bytes) %dx%d (%d)\n", index, (int)iconWidth, (int)iconHeight, (int)iconBpp, (int)iconDpiWidth, (int)iconDpiHeight, (int)CFDataGetLength(myImageStream), (int)iconSelectedHeight, (int)iconSelectedWidth, (int)iconSelected);
#endif
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
                    CGImageDestinationFinalize(myImage);
                        
                    
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
                    icon = myICOFile;
                    *iconsize = (CFDataGetLength(myImageStream) + sizeof(iconDir));
                }
            }
        }
    }
}

// Returned in UUID binary format
void getUpnpUuid(CFUUIDBytes *uuid){
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                              IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFStringRef uuidStr = IORegistryEntryCreateCFProperty(platformExpert,
                                                           CFSTR(kIOPlatformUUIDKey),
                                                           kCFAllocatorDefault, 0);
        
        IOObjectRelease(platformExpert);
        
        uuid = malloc(sizeof(CFUUIDBytes));
        *uuid = CFUUIDGetUUIDBytes(CFUUIDCreateFromString(kCFAllocatorDefault, uuidStr));
//        uuid =
        return;
        
    } else uuid = NULL;
}
/*
 int BIWiFiInterface::syncSIOCSIFFLAGS(IONetworkController * ctr)
 {
 UInt16    flags = getFlags();
 IOReturn  ret   = kIOReturnSuccess;
 
 if ( ( ((flags & IFF_UP) == 0) || _controllerLostPower ) &&
 ( flags & IFF_RUNNING ) )
 {
 // If interface is marked down and it is currently running,
 // then stop it.
 
 ctr->doDisable(this);
 flags &= ~IFF_RUNNING;
 _ctrEnabled = false;
 }
 else if ( ( flags & IFF_UP )                &&
 ( _controllerLostPower == false ) &&
 ((flags & IFF_RUNNING) == 0) )
 {
 // If interface is marked up and it is currently stopped,
 // then start it.
 
 if ( (ret = enableController(ctr)) == kIOReturnSuccess )
 flags |= IFF_RUNNING;
 }
 
 if ( flags & IFF_RUNNING )
 {
 IOReturn rc;
 
 // We don't expect multiple flags to be changed for a given
 // SIOCSIFFLAGS call.
 
 // Promiscuous mode
 
 rc = (flags & IFF_PROMISC) ?
 enableFilter(ctr, gIONetworkFilterGroup,
 kIOPacketFilterPromiscuous) :
 disableFilter(ctr, gIONetworkFilterGroup,
 kIOPacketFilterPromiscuous);
 
 if (ret == kIOReturnSuccess) ret = rc;
 
 // Multicast-All mode
 
 rc = (flags & IFF_ALLMULTI) ?
 enableFilter(ctr, gIONetworkFilterGroup,
 kIOPacketFilterMulticastAll) :
 disableFilter(ctr, gIONetworkFilterGroup,
 kIOPacketFilterMulticastAll);
 
 if (ret == kIOReturnSuccess) ret = rc;
 }
 
 // Update the flags field to pick up any modifications. Also update the
 // property table to reflect any flag changes.
 
 setFlags(flags, ~flags);
 
 return errnoFromReturn(ret);
 }*/

