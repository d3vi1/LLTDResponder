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
        IOObjectRelease(platformExpert);

        CFDictionaryRef utiDecl = UTTypeCopyDeclaration(UniformTypeIdentifier);
        if (utiDecl){
            CFStringRef iconFileNameRef = CFDictionaryGetValue(utiDecl, CFSTR("UTTypeIconFile"));
            // if there is no icon for this UTI, load the icon for the key conformsTo UTI.
            // cycle until we find one, there is always one at the top of the tree.
            if (nil == iconFileNameRef){
                while ((nil==iconFileNameRef) && (nil != utiDecl)){
                    UniformTypeIdentifier = CFDictionaryGetValue(utiDecl, CFSTR("UTTypeConformsTo"));
                    utiDecl = UTTypeCopyDeclaration(UniformTypeIdentifier);
                    iconFileNameRef = CFDictionaryGetValue(utiDecl, CFSTR("UTTypeIconFile"));
                }
          }

            CFBundleRef CTBundle = CFBundleCreate(kCFAllocatorDefault, UTTypeCopyDeclaringBundleURL(UniformTypeIdentifier));
            CFURLRef iconURL = CFBundleCopyResourceURL(CTBundle, iconFileNameRef, NULL, NULL);
            printf("Icon URL: %s\n", CFStringGetCStringPtr(CFURLGetString(iconURL), kCFStringEncodingUTF8));
            
/*            // Load the icns file
            CGImageSourceRef myIcon = CGImageSourceCreateWithURL(iconURL, NULL);
            
            if(myIcon!=NULL){
                size_t iconCount = CGImageSourceGetCount (myIcon);
                printf("icon count: %zu\n", iconCount);
                if(iconCount > 0) {
                    for(int index=0; index<iconCount; index++){
                        CGImageRef myImage = CGImageSourceCreateImageAtIndex(myIcon, index, NULL);
                        
                        CFDictionaryRef imageProperties = CGImageSourceCopyPropertiesAtIndex(myIcon, index, (CFDictionaryRef) options);
                        printf("Icon index(%d): %dx%d %dbpp\n", index, (UInt32)CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelWidth), (UInt32)CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelHeight), (UInt32)CFDictionaryGetValue(imageProperties, kCGImagePropertyDepth));
                        CFRelease(imageProperties);
                    }
                }
            }
            
            //CGDataProviderRef imageDataProvRev CGImageGetDataProvider (CGImageRef icon);
//            if (myIcon){
//                CFDictionaryRef *selectProps();
//
//            }
            sleep(1);*/
        }

    }
    
}


void getUpnpUuid(void *data){
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                              IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFTypeRef uuid = IORegistryEntryCreateCFProperty(platformExpert,
                                                           CFSTR(kIOPlatformUUIDKey),
                                                           kCFAllocatorDefault, 0);
        
        IOObjectRelease(platformExpert);
    }
}
