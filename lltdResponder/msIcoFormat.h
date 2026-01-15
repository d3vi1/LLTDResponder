/******************************************************************************
 *                                                                            *
 *   msIcoFormat.h                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 09.02.2016.                      *
 *   Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef msIcoFormat_h
#define msIcoFormat_h

#include <stdint.h>

//
// Windows ICO file description from MSDN, adapted to OS X data types
// See: http://msdn.microsoft.com/en-us/library/ms997538.aspx
//
#pragma pack( push )
#pragma pack( 2 )
typedef struct {
    uint8_t      Width;            // Width, in pixels, of the image
    uint8_t      Height;           // Height, in pixels, of the image
    uint8_t      ColorCount;       // Number of colors in image (0 if >=8bpp)
    uint8_t      Reserved;         // Reserved ( must be 0)
    uint16_t     Planes;           // Color Planes
    uint16_t     BitCount;         // Bits per pixel
    uint32_t     BytesInRes;       // How many bytes in this resource?
    uint32_t     ImageOffset;      // Where in the file is this image?
} iconDirEntry;

typedef struct {
    uint16_t        idReserved;    // Reserved (must be 0)
    uint16_t        idType;        // Resource Type (1 for icons)
    uint16_t        idCount;       // How many images?
    iconDirEntry    idEntries[1];  // An entry for each image (idCount of 'em)
} iconDir;
#pragma pack( pop )

#define icoDirTypeIcon 1
#define icoDirTypeCursor 2


#endif /* msIcoFormat_h */
