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

void getMachineName(void *data);
void getSupportName(void *data);
void getFriendlyName(void *data);
const char * getUpnpUuid();
void getHwId(void *data);
void getIconImage();
void getJumboIcon(void *data);


//
// Windows ICO file description from MSDN, adapted to OS X data types
// See: http://msdn.microsoft.com/en-us/library/ms997538.aspx
//
typedef struct {
    UInt8        Width;         // Width, in pixels, of the image
    UInt8        Height;        // Height, in pixels, of the image
    UInt8        ColorCount;    // Number of colors in image (0 if >=8bpp)
    UInt8        Reserved;      // Reserved ( must be 0)
    UInt16       Planes;        // Color Planes
    UInt16       BitCount;      // Bits per pixel
    UInt32       BytesInRes;    // How many bytes in this resource?
    UInt32       ImageOffset;   // Where in the file is this image?
} iconDirEntry;

typedef struct {
    UInt16          idReserved;   // Reserved (must be 0)
    UInt16          idType;       // Resource Type (1 for icons)
    UInt16          idCount;      // How many images?
    iconDirEntry    idEntries[1]; // An entry for each image (idCount of 'em)
} iconDir;

#define icoDirTypeIcon 0
#define icoDirTypeCursor 1

#endif
