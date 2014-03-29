/******************************************************************************
 *                                                                            *
 *   darwin-ops.c                                                             *
 *   LLMNRd                                                                   *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 24.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/

#ifndef LLMNRd_darwin_ops_h
#define LLMNRd_darwin_ops_h

#pragma mark Functions that return machine information
// Returned in UCS-2LE
// SCDynamicStoreCopyLocalHostName
void getMachineName(char **data, size_t *stringSize);
// Returned in UCS-2LE only with QueryLargeTLV
// SCDynamicStoreCopyComputerName
void getFriendlyName(char **data, size_t *stringSize);
// Returned in UCS-2LE
void getSupportInfo(void *data);
// Returned in UUID binary format
void getUpnpUuid(CFUUIDBytes *uuid);
// See 2.2.2.3 in UCS-2LE
void getHwId(void *data);
// Returns a copy of the icon image
void getIconImage(void *icon, size_t *iconsize);
// Only with QueryLargeTLV
void getDetailedIconImage(void *data);
void getHostCharacteristics (void *data);
//Only with QueryLargeTLV
void getComponentTable(void *data);
void getPerformanceCounterFrequency(void *data);
#pragma mark Functions that are interface specific
#pragma mark -

//gets the IPv4 address if one is available
//boolean getIfIPv4info

//boolean getIfIPv6info

//Type 0x2 uint8, Length 0x2 uint8, Bits: public, private, duplex, hasmanagement, loopback, reserved[11]
//boolean getIfCharacteristics


//boolean getQosCharacteristics

//gets the IANAifType
//void getIfPhysicalMedium

//void WgetWirelessMode

//void WgetIfRSSI

//void WgetIfSSID

//void WgetIfBSSID

//void WgetIfMaxRate

//void WgetIfPhyMedium

//void WgetApAssociationTable

#pragma mark Microsoft ICO file description
#pragma mark -
//
// Windows ICO file description from MSDN, adapted to OS X data types
// See: http://msdn.microsoft.com/en-us/library/ms997538.aspx
//
#pragma pack( push )
#pragma pack( 2 )
typedef struct {
    UInt8        Width;            // Width, in pixels, of the image
    UInt8        Height;           // Height, in pixels, of the image
    UInt8        ColorCount;       // Number of colors in image (0 if >=8bpp)
    UInt8        Reserved;         // Reserved ( must be 0)
    UInt16       Planes;           // Color Planes
    UInt16       BitCount;         // Bits per pixel
    UInt32       BytesInRes;       // How many bytes in this resource?
    UInt32       ImageOffset;      // Where in the file is this image?
} iconDirEntry;
#pragma pack( pop )

#pragma pack( push )
#pragma pack( 2 )
typedef struct {
    UInt16          idReserved;    // Reserved (must be 0)
    UInt16          idType;        // Resource Type (1 for icons)
    UInt16          idCount;       // How many images?
    iconDirEntry    idEntries[1];  // An entry for each image (idCount of 'em)
/*    UInt8           Width;         // Width, in pixels, of the image
    UInt8           Height;        // Height, in pixels, of the image
    UInt8           ColorCount;    // Number of colors in image (0 if >=8bpp)
    UInt8           Reserved;      // Reserved ( must be 0)
    UInt16          Planes;        // Color Planes
    UInt16          BitCount;      // Bits per pixel
    UInt32          BytesInRes;    // How many bytes in this resource?
    UInt32          ImageOffset;   // Where in the file is this image?*/
} iconDir;
#pragma pack( pop )

#define icoDirTypeIcon 1
#define icoDirTypeCursor 2

#pragma mark -
#endif
