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

void getIconImage(){

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


            printf("Icon URL: %s\n", CFStringGetCStringPtr(CFURLGetString(iconURL), kCFStringEncodingUTF8));
            
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
                printf("icon count: %zu\n", iconCount);
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
                        if (((iconSelectedWidth > iconWidth)&(iconWidth > 48)) | (haveIconSelected == FALSE)){
                                iconSelectedHeight = iconHeight;
                                iconSelectedWidth = iconWidth;
                                iconSelected = index;
                                haveIconSelected = TRUE;
                        }
                        
                        printf("Icon index(%d): %4dx%4d %dbpp (%03dx%03d DPI)(%d bytes) %dx%d (%d)\n", index, (int)iconWidth, (int)iconHeight, (int)iconBpp, (int)iconDpiWidth, (int)iconDpiHeight, (int)CFDataGetLength(myImageStream), (int)iconSelectedHeight, (int)iconSelectedWidth, (int)iconSelected);
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
                    iconDir *myICOFile;
                    myICOFile = malloc(CFDataGetLength(myImageStream) + sizeof(iconDir) + sizeof(iconDirEntry));
                    myICOFile->idCount = 1;
                    myICOFile->idReserved = 0;
                    myICOFile->idType = icoDirTypeIcon;
                    myICOFile->idEntries->Height=48;
                    myICOFile->idEntries->Width=48;
                    myICOFile->idEntries->ColorCount=0;
                    myICOFile->idEntries->Reserved=0;
                    myICOFile->idEntries->Planes=0;
                    myICOFile->idEntries->BitCount=32;
                    myICOFile->idEntries->BytesInRes=(UInt32)CFDataGetLength(myImageStream);
                    myICOFile->idEntries->ImageOffset=sizeof(iconDir)+sizeof(iconDirEntry);
                    CFDataGetBytes(myImageStream,  CFRangeMake(0,CFDataGetLength(myImageStream)), (void *)myICOFile + myICOFile->idEntries->ImageOffset);
                    CFDataRef myICOFileData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, myICOFile, (CFDataGetLength(myImageStream) + sizeof(iconDir) + sizeof(iconDirEntry)), kCFAllocatorDefault);
                    
                    //TODO: Insert here the code to release the object blah blah blah
                    
                    
                    //Clean-up mode
                    CFRelease(myICOFileData);
                    free(myICOFile);
                    
                }
            }
        }
    }
}

const char * getUpnpUuid(){
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                              IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFStringRef uuid = IORegistryEntryCreateCFProperty(platformExpert,
                                                           CFSTR(kIOPlatformUUIDKey),
                                                           kCFAllocatorDefault, 0);
        
        IOObjectRelease(platformExpert);
        
        return CFStringGetCStringPtr(uuid, kCFStringEncodingASCII);
    }
    
    return NULL;
}
