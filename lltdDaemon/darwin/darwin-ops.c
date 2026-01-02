/******************************************************************************
 *                                                                            *
 *   darwin-ops.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 24.03.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#if defined(__APPLE__) && defined(__has_include)
#if __has_include(<MobileCoreServices/UTType.h>)
#include <MobileCoreServices/UTType.h>
#elif __has_include(<CoreServices/UTType.h>)
#include <CoreServices/UTType.h>
#endif
#endif

#ifndef kIOMasterPortDefault
#define kIOMasterPortDefault kIOMainPortDefault
#endif

static mach_port_t lltdIOMasterPort(void) {
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
    return kIOMainPortDefault;
#else
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
    if (__builtin_available(macOS 12.0, *)) {
        return kIOMainPortDefault;
    }
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return kIOMasterPortDefault;
#pragma clang diagnostic pop
#endif
}

#pragma mark Functions that return machine information
#pragma mark -


#pragma mark Complete
//==============================================================================
//
// Returned in UUID binary format
//
//==============================================================================
void getUpnpUuid(void **pointer){
    io_service_t platformExpert = IOServiceGetMatchingService(lltdIOMasterPort(), IOServiceMatching("IOPlatformExpertDevice"));
    
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
        
    } else {
        *pointer = NULL;
    }
}


static CFURLRef copyDeviceIconURL(void) {
    io_service_t platformExpert = IOServiceGetMatchingService(lltdIOMasterPort(), IOServiceMatching("IOPlatformExpertDevice"));
    CFDataRef modelCodeData = NULL;
    if (platformExpert) {
        modelCodeData = IORegistryEntryCreateCFProperty(platformExpert,
                                                        CFSTR("model"),
                                                        kCFAllocatorDefault,
                                                        kNilOptions);
        IOObjectRelease(platformExpert);
    }

    CFStringRef modelCode = NULL;
    if (modelCodeData) {
        modelCode = CFStringCreateWithBytes(kCFAllocatorDefault,
                                            CFDataGetBytePtr(modelCodeData),
                                            CFDataGetLength(modelCodeData),
                                            kCFStringEncodingASCII,
                                            false);
    }

    CFStringRef uti = NULL;
    if (modelCode && CFStringGetLength(modelCode) > 0) {
        uti = UTTypeCreatePreferredIdentifierForTag(CFSTR("com.apple.device-model-code"), modelCode, NULL);
    } else {
        uti = CFRetain(CFSTR("com.apple.mac"));
    }

    if (modelCode) {
        CFRelease(modelCode);
    }
    if (modelCodeData) {
        CFRelease(modelCodeData);
    }

    if (!uti) {
        return NULL;
    }

    if (CFStringHasPrefix(uti, CFSTR("dyn"))) {
        CFRelease(uti);
        uti = CFRetain(CFSTR("com.apple.mac"));
    }

    CFStringRef workingUTI = CFRetain(uti);
    CFRelease(uti);

    while (workingUTI) {
        CFDictionaryRef declaration = UTTypeCopyDeclaration(workingUTI);
        if (!declaration) {
            CFRelease(workingUTI);
            break;
        }

        CFStringRef iconFileName = CFDictionaryGetValue(declaration, CFSTR("UTTypeIconFile"));
        if (iconFileName) {
            CFRetain(iconFileName);
            CFURLRef bundleURL = UTTypeCopyDeclaringBundleURL(workingUTI);
            CFBundleRef bundle = NULL;
            if (bundleURL) {
                bundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);
            }
            CFURLRef iconURL = NULL;
            if (bundle) {
                iconURL = CFBundleCopyResourceURL(bundle, iconFileName, NULL, NULL);
            }
            if (bundleURL) {
                CFRelease(bundleURL);
            }
            if (bundle) {
                CFRelease(bundle);
            }
            CFRelease(iconFileName);
            CFRelease(declaration);
            CFRelease(workingUTI);
            return iconURL;
        }

        CFTypeRef conformsTo = CFDictionaryGetValue(declaration, CFSTR("UTTypeConformsTo"));
        CFStringRef nextUTI = NULL;
        if (conformsTo) {
            if (CFGetTypeID(conformsTo) == CFArrayGetTypeID()) {
                CFArrayRef conformsArray = (CFArrayRef)conformsTo;
                if (CFArrayGetCount(conformsArray) > 0) {
                    nextUTI = CFArrayGetValueAtIndex(conformsArray, 0);
                }
            } else if (CFGetTypeID(conformsTo) == CFStringGetTypeID()) {
                nextUTI = (CFStringRef)conformsTo;
            }
        }

        if (nextUTI) {
            CFRetain(nextUTI);
        }
        CFRelease(declaration);
        CFRelease(workingUTI);
        workingUTI = nextUTI;
    }

    return NULL;
}

static size_t selectIconIndex(CGImageSourceRef source, uint32_t targetSize) {
    size_t iconCount = CGImageSourceGetCount(source);
    size_t selectedIndex = 0;
    uint32_t selectedWidth = 0;
    bool found = false;

    for (size_t index = 0; index < iconCount; index++) {
        CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(source, index, NULL);
        if (!properties) {
            continue;
        }
        uint32_t iconWidth = 0;
        uint32_t iconHeight = 0;
        CFNumberGetValue(CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth), kCFNumberIntType, &iconWidth);
        CFNumberGetValue(CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight), kCFNumberIntType, &iconHeight);

        if (iconWidth >= targetSize && iconHeight >= targetSize) {
            if (!found || iconWidth < selectedWidth) {
                selectedIndex = index;
                selectedWidth = iconWidth;
                found = true;
            }
        } else if (!found && iconWidth > selectedWidth) {
            selectedIndex = index;
            selectedWidth = iconWidth;
        }
        CFRelease(properties);
    }

    return selectedIndex;
}

static CFDataRef copyPngThumbnail(CGImageSourceRef source, size_t index, uint32_t size) {
    CFDictionaryRef options = NULL;
    CFStringRef keys[2];
    CFTypeRef values[2];
    CFNumberRef thumbSizeRef = CFNumberCreate(NULL, kCFNumberIntType, &size);
    keys[0] = kCGImageSourceCreateThumbnailFromImageIfAbsent;
    values[0] = kCFBooleanTrue;
    keys[1] = kCGImageSourceThumbnailMaxPixelSize;
    values[1] = thumbSizeRef;
    options = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CGImageRef thumbnail = CGImageSourceCreateThumbnailAtIndex(source, index, options);
    CFRelease(options);
    CFRelease(thumbSizeRef);
    if (!thumbnail) {
        return NULL;
    }

    CFMutableDataRef imageStream = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(imageStream, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(destination, thumbnail, NULL);
    CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CFRelease(thumbnail);

    return imageStream;
}

static bool buildIcoFromPngs(CFArrayRef pngs, const uint32_t *sizes, size_t count, void **icon, size_t *iconsize) {
    size_t headerSize = sizeof(iconDir) + (count - 1) * sizeof(iconDirEntry);
    size_t totalSize = headerSize;
    for (size_t i = 0; i < count; i++) {
        totalSize += CFDataGetLength(CFArrayGetValueAtIndex(pngs, i));
    }

    uint8_t *icoBytes = malloc(totalSize);
    if (!icoBytes) {
        return false;
    }

    iconDir *dir = (iconDir *)icoBytes;
    dir->idReserved = 0;
    dir->idType = CFSwapInt16HostToLittle(icoDirTypeIcon);
    dir->idCount = CFSwapInt16HostToLittle((UInt16)count);

    size_t offset = headerSize;
    for (size_t i = 0; i < count; i++) {
        CFDataRef pngData = CFArrayGetValueAtIndex(pngs, i);
        uint32_t size = sizes[i];
        iconDirEntry *entry = &dir->idEntries[i];
        entry->Width = (size == 256) ? 0 : (UInt8)size;
        entry->Height = (size == 256) ? 0 : (UInt8)size;
        entry->ColorCount = 0;
        entry->Reserved = 0;
        entry->Planes = CFSwapInt16HostToLittle(1);
        entry->BitCount = CFSwapInt16HostToLittle(32);
        entry->BytesInRes = CFSwapInt32HostToLittle((UInt32)CFDataGetLength(pngData));
        entry->ImageOffset = CFSwapInt32HostToLittle((UInt32)offset);
        CFDataGetBytes(pngData, CFRangeMake(0, CFDataGetLength(pngData)), icoBytes + offset);
        offset += CFDataGetLength(pngData);
    }

    *icon = icoBytes;
    *iconsize = totalSize;
    return true;
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
    *icon = NULL;
    *iconsize = 0;
    CFURLRef iconURL = copyDeviceIconURL();
    if (!iconURL) {
        return;
    }

    CGImageSourceRef source = CGImageSourceCreateWithURL(iconURL, NULL);
    CFRelease(iconURL);
    if (!source) {
        return;
    }

    size_t selectedIndex = selectIconIndex(source, 48);
    CFDataRef pngData = copyPngThumbnail(source, selectedIndex, 48);
    if (!pngData) {
        CFRelease(source);
        return;
    }

    CFArrayRef pngs = CFArrayCreate(kCFAllocatorDefault, (const void **)&pngData, 1, &kCFTypeArrayCallBacks);
    uint32_t sizes[] = { 48 };
    buildIcoFromPngs(pngs, sizes, 1, icon, iconsize);
    CFRelease(pngs);
    CFRelease(pngData);
    CFRelease(source);
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
    io_service_t platformExpert = IOServiceGetMatchingService(lltdIOMasterPort(), IOServiceMatching("IOPlatformExpertDevice"));
    
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
        return false;
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
                return false;
            } else {
                free(MediaList);
                return true;
            }
        }
    }
    free(MediaList);
    return false;
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

static CFTypeRef copyWiFiProperty(const char *interfaceName, CFStringRef key) {
    if (!interfaceName) {
        return NULL;
    }

    mach_port_t masterPort = lltdIOMasterPort();
    io_service_t interfaceService = IOServiceGetMatchingService(masterPort,
                                                                IOBSDNameMatching(masterPort, 0, interfaceName));
    if (!interfaceService) {
        return NULL;
    }

    CFTypeRef value = IORegistryEntrySearchCFProperty(interfaceService,
                                                      kIOServicePlane,
                                                      key,
                                                      kCFAllocatorDefault,
                                                      kIORegistryIterateRecursively);
    IOObjectRelease(interfaceService);
    return value;
}

static boolean_t copySSIDFromProperty(CFTypeRef value, char **data, size_t *dataSize) {
    if (!value || !data || !dataSize) {
        return false;
    }

    if (CFGetTypeID(value) == CFDataGetTypeID()) {
        CFDataRef dataValue = (CFDataRef)value;
        CFIndex length = CFDataGetLength(dataValue);
        if (length <= 0) {
            return false;
        }

        char *buffer = malloc((size_t)length + 1);
        if (!buffer) {
            return false;
        }
        memcpy(buffer, CFDataGetBytePtr(dataValue), (size_t)length);
        buffer[length] = '\0';
        *data = buffer;
        *dataSize = (size_t)length;
        return true;
    }

    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        CFStringRef stringValue = (CFStringRef)value;
        CFIndex length = CFStringGetLength(stringValue);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
        char *buffer = malloc((size_t)maxSize + 1);
        if (!buffer) {
            return false;
        }
        if (!CFStringGetCString(stringValue, buffer, maxSize + 1, kCFStringEncodingUTF8)) {
            free(buffer);
            return false;
        }
        *dataSize = strlen(buffer);
        *data = buffer;
        return true;
    }

    return false;
}

boolean_t getSSID(char **data, size_t *dataSize, void *networkInterface) {
    if (!data || !dataSize) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    if (!currentNetworkInterface) {
        return false;
    }

    CFTypeRef ssidValue = copyWiFiProperty(currentNetworkInterface->deviceName, CFSTR("IO80211SSID"));
    if (!ssidValue) {
        return false;
    }

    boolean_t ok = copySSIDFromProperty(ssidValue, data, dataSize);
    CFRelease(ssidValue);
    return ok;
}

boolean_t getWifiMaxRate(uint32_t *rateMbps, void *networkInterface) {
    if (!rateMbps) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    CFTypeRef value = copyWiFiProperty(currentNetworkInterface->deviceName, CFSTR("IO80211MaxLinkSpeed"));
    if (!value) {
        return false;
    }

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        CFNumberGetValue(value, kCFNumberSInt32Type, rateMbps);
        CFRelease(value);
        return true;
    }

    CFRelease(value);
    return false;
}

boolean_t getWifiRssi(int8_t *rssi, void *networkInterface) {
    if (!rssi) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    CFTypeRef value = copyWiFiProperty(currentNetworkInterface->deviceName, CFSTR("IO80211RSSI"));
    if (!value) {
        value = copyWiFiProperty(currentNetworkInterface->deviceName, CFSTR("RSSI"));
    }
    if (!value) {
        return false;
    }

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        int32_t rssiValue = 0;
        CFNumberGetValue(value, kCFNumberSInt32Type, &rssiValue);
        *rssi = (int8_t)rssiValue;
        CFRelease(value);
        return true;
    }

    CFRelease(value);
    return false;
}

boolean_t getWifiPhyMedium(uint32_t *phyMedium, void *networkInterface) {
    if (!phyMedium) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    CFTypeRef value = copyWiFiProperty(currentNetworkInterface->deviceName, CFSTR("IO80211PHYMode"));
    if (!value) {
        return false;
    }

    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        CFNumberGetValue(value, kCFNumberSInt32Type, phyMedium);
        CFRelease(value);
        return true;
    }

    CFRelease(value);
    return false;
}

boolean_t getIfIPv4info(uint32_t *ipv4, void *networkInterface) {
    if (!ipv4) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    if (!currentNetworkInterface) {
        return false;
    }

    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) {
        return false;
    }

    boolean_t found = false;
    struct ifaddrs *temp_addr = interfaces;
    while (temp_addr != NULL) {
        if (temp_addr->ifa_addr && temp_addr->ifa_addr->sa_family == AF_INET &&
            strcmp(currentNetworkInterface->deviceName, temp_addr->ifa_name) == 0) {
            *ipv4 = ((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr.s_addr;
            found = true;
            break;
        }
        temp_addr = temp_addr->ifa_next;
    }

    freeifaddrs(interfaces);
    return found;
}

boolean_t getIfIPv6info(struct in6_addr *ipv6, void *networkInterface) {
    if (!ipv6) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    if (!currentNetworkInterface) {
        return false;
    }

    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) {
        return false;
    }

    boolean_t found = false;
    struct ifaddrs *temp_addr = interfaces;
    while (temp_addr != NULL) {
        if (temp_addr->ifa_addr && temp_addr->ifa_addr->sa_family == AF_INET6 &&
            strcmp(currentNetworkInterface->deviceName, temp_addr->ifa_name) == 0) {
            *ipv6 = ((struct sockaddr_in6 *)temp_addr->ifa_addr)->sin6_addr;
            found = true;
            break;
        }
        temp_addr = temp_addr->ifa_next;
    }

    freeifaddrs(interfaces);
    return found;
}

boolean_t getIfCharacteristics(uint16_t *characteristics, void *networkInterface) {
    if (!characteristics) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    if (!currentNetworkInterface) {
        return false;
    }

    uint16_t value = 0;
    if (currentNetworkInterface->MediumType & IFM_FDX) {
        value |= Config_TLV_NetworkInterfaceDuplex_Value;
    }
    if (currentNetworkInterface->flags & IFF_LOOPBACK) {
        value |= Config_TLV_InterfaceIsLoopback_Value;
    }

    *characteristics = value;
    return true;
}

boolean_t getQosCharacteristics(uint16_t *characteristics) {
    if (!characteristics) {
        return false;
    }

    *characteristics = Config_TLV_QOS_L2Fwd | Config_TLV_QOS_PrioTag | Config_TLV_QOS_VLAN;
    return true;
}

boolean_t getIfIANAType(uint32_t *ifType, void *networkInterface) {
    if (!ifType) {
        return false;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    if (!currentNetworkInterface) {
        return false;
    }

    *ifType = currentNetworkInterface->ifType;
    return true;
}

void getIfPhysicalMedium(uint32_t *mediumType, void *networkInterface) {
    if (!mediumType) {
        return;
    }

    network_interface_t *currentNetworkInterface = (network_interface_t *)networkInterface;
    if (!currentNetworkInterface) {
        return;
    }

    *mediumType = (uint32_t)currentNetworkInterface->MediumType;
}

boolean_t WgetWirelessMode(uint8_t *mode, void *networkInterface) {
    if (!mode) {
        return false;
    }

    *mode = (uint8_t)getWifiMode(networkInterface);
    return true;
}

boolean_t WgetIfRSSI(int8_t *rssi, void *networkInterface) {
    return getWifiRssi(rssi, networkInterface);
}

boolean_t WgetIfSSID(char **ssid, size_t *ssidSize, void *networkInterface) {
    return getSSID(ssid, ssidSize, networkInterface);
}

boolean_t WgetIfBSSID(void **bssid, void *networkInterface) {
    return getBSSID(bssid, networkInterface);
}

boolean_t WgetIfMaxRate(uint32_t *rateMbps, void *networkInterface) {
    return getWifiMaxRate(rateMbps, networkInterface);
}

boolean_t WgetIfPhyMedium(uint32_t *phyMedium, void *networkInterface) {
    return getWifiPhyMedium(phyMedium, networkInterface);
}

void WgetApAssociationTable(void **data, size_t *dataSize, void *networkInterface) {
    (void)networkInterface;
    if (data) {
        *data = NULL;
    }
    if (dataSize) {
        *dataSize = 0;
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
void getHwId(void *data){
    if (!data) {
        return;
    }

    io_service_t platformExpert = IOServiceGetMatchingService(lltdIOMasterPort(), IOServiceMatching("IOPlatformExpertDevice"));
    if (!platformExpert) {
        return;
    }

    CFStringRef platformUUID = IORegistryEntryCreateCFProperty(platformExpert,
                                                               CFSTR(kIOPlatformUUIDKey),
                                                               kCFAllocatorDefault,
                                                               0);
    IOObjectRelease(platformExpert);

    if (!platformUUID) {
        return;
    }

    const size_t maxSize = 64;
    memset(data, 0, maxSize);
    CFIndex length = CFStringGetLength(platformUUID);
    CFStringGetBytes(platformUUID, CFRangeMake(0, length), kCFStringEncodingUTF16LE, 0, false, data, maxSize, NULL);
    CFRelease(platformUUID);
}


//==============================================================================
//
// Only with QueryLargeTLV
// Returns a copy of the icon image in Microsoft ICO format
// at the followin sizes: 16x16, 32x32, 64x64, 128x128, 256x256
// and if available, 24x24 and 48x48
//
//==============================================================================
void getDetailedIconImage (void **data, size_t *iconsize){
    *data = NULL;
    *iconsize = 0;
    CFURLRef iconURL = copyDeviceIconURL();
    if (!iconURL) {
        return;
    }

    CGImageSourceRef source = CGImageSourceCreateWithURL(iconURL, NULL);
    CFRelease(iconURL);
    if (!source) {
        return;
    }

    uint32_t sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
    size_t sizeCount = sizeof(sizes) / sizeof(sizes[0]);
    CFMutableArrayRef pngs = CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)sizeCount, &kCFTypeArrayCallBacks);
    uint32_t usedSizes[7];
    size_t usedCount = 0;

    for (size_t i = 0; i < sizeCount; i++) {
        size_t index = selectIconIndex(source, sizes[i]);
        CFDataRef pngData = copyPngThumbnail(source, index, sizes[i]);
        if (!pngData) {
            continue;
        }
        CFArrayAppendValue(pngs, pngData);
        usedSizes[usedCount++] = sizes[i];
        CFRelease(pngData);
    }

    if (usedCount > 0) {
        buildIcoFromPngs(pngs, usedSizes, usedCount, data, iconsize);
    }
    CFRelease(pngs);
    CFRelease(source);
}


//==============================================================================
//
// Hmm... This will take a bit of work
// I know that we are a host that is
// hub+switch
//
//==============================================================================
void getHostCharacteristics (void *data){
    if (!data) {
        return;
    }
    uint16_t *characteristics = data;
    *characteristics = htons(Config_TLV_HasManagementURL_Value);
}


//==============================================================================
//
// Only with QueryLargeTLV
//
//==============================================================================
void getComponentTable (void **data, size_t *dataSize){
    *data = NULL;
    *dataSize = 0;
}


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
