/******************************************************************************
 *                                                                            *
 *   linux-ops.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 02.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/

#include "lltdDaemon.h"

#pragma mark Functions that return machine information
#pragma mark -


//==============================================================================
//
// Returned in UUID binary format
//
//==============================================================================
void getUpnpUuid(void **pointer);



//==============================================================================
//
// Returns a copy of the icon image in Microsoft ICO format.
// Made from an image larger or equal to 48px in size.
// The icon must be smaller than 32kb which we are not yet verifying.
// Only with QueryLargetTLV
//
//==============================================================================
void getIconImage(void **icon, size_t *iconsize);



//==============================================================================
//
// Returned in UCS-2LE
//
//==============================================================================
void getMachineName(char **pointer, size_t *stringSize);



//==============================================================================
//
// Returned in UCS-2LE only with QueryLargeTLV
//
//==============================================================================
void getFriendlyName(char **pointer, size_t *stringSize);



//==============================================================================
//
// Returned in UCS-2LE
// Returns the following URL with the last 3 or 4 digits of the serial number
// at the CC argument http://support-sp.apple.com/sp/index?page=psp&cc=XXX
// Taken from:
// /Applications/Utilities/System Profiler.app/Contents/Resources/SupportLinks.strings
//
//==============================================================================
void getSupportInfo(void **data, size_t *stringSize);



//==============================================================================
//
// Returns
// TRUE  = Ad-Hoc or disconnected
// FALSE = Infrastructure mode
//
//==============================================================================
boolean_t getWifiMode(void *networkInterface);



//==============================================================================
//
// Returns a copy of the 6 bytes comprising the BSSID.
//
//==============================================================================
boolean_t getBSSID (void **data, void *networkInterface);



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
void getDetailedIconImage (void **data, size_t *iconsize);


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
//
//==============================================================================
void setPromiscuous(void *networkInterface, boolean_t set);

