/******************************************************************************
 *                                                                            *
 *   lltd_wifi_iokit.c                                                        *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Pure C implementation of Wi-Fi metrics using IOKit.                      *
 *   No Objective-C, no ARC, no libarclite dependency.                        *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 07.01.2026.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#include "lltd_wifi_corewlan.h"
#include "lltd_wifi_rates.h"

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#include <stdlib.h>

/* IOKit property keys for Wi-Fi (IO80211) */
#define kIO80211RSSI            "IO80211RSSI"
#define kIO80211Noise           "IO80211Noise"
#define kIO80211LinkSpeed       "IO80211LinkSpeed"
#define kIO80211TxRate          "IO80211TxRate"
#define kIO80211PHYMode         "IO80211PHYMode"
#define kIO80211ChannelWidth    "IO80211ChannelWidth"
#define kIO80211Channel         "IO80211Channel"
#define kIO80211BSSID           "IO80211BSSID"

/*
 * Search for a property in the IOKit registry.
 * Searches both children (legacy) and parents (Skywalk on Tahoe+).
 */
static CFTypeRef copyWiFiPropertyByName(const char *interfaceName, CFStringRef key) {
    if (!interfaceName || !key) {
        return NULL;
    }

    /* Get the interface service by BSD name */
    io_service_t interfaceService = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOBSDNameMatching(kIOMainPortDefault, 0, interfaceName)
    );
    if (!interfaceService) {
        return NULL;
    }

    /* First try searching children (legacy IOKit stack) */
    CFTypeRef value = IORegistryEntrySearchCFProperty(
        interfaceService,
        kIOServicePlane,
        key,
        kCFAllocatorDefault,
        kIORegistryIterateRecursively
    );

    /* If not found, search parents (Skywalk stack on Tahoe+) */
    if (!value) {
        value = IORegistryEntrySearchCFProperty(
            interfaceService,
            kIOServicePlane,
            key,
            kCFAllocatorDefault,
            kIORegistryIterateRecursively | kIORegistryIterateParents
        );
    }

    IOObjectRelease(interfaceService);
    return value;
}

/*
 * Get an integer value from an IOKit property.
 * Returns 0 and sets *found=false if not found or wrong type.
 */
static int getIntProperty(const char *ifname, const char *key, int *found) {
    int result = 0;
    *found = 0;

    CFStringRef cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (!cfKey) {
        return 0;
    }

    CFTypeRef value = copyWiFiPropertyByName(ifname, cfKey);
    CFRelease(cfKey);

    if (value) {
        if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &result);
            *found = 1;
        }
        CFRelease(value);
    }

    return result;
}

/*
 * Check if interface is associated (has a BSSID)
 */
static int isAssociated(const char *ifname) {
    CFTypeRef bssid = copyWiFiPropertyByName(ifname, CFSTR(kIO80211BSSID));
    if (bssid) {
        CFRelease(bssid);
        return 1;
    }
    return 0;
}

/*
 * Map IOKit PHY mode value to our constants.
 * IOKit uses same values as CWPHYMode.
 */
static int mapPhyMode(int iokitPhyMode) {
    switch (iokitPhyMode) {
        case 1:  return LLTD_WIFI_PHY_11A;
        case 2:  return LLTD_WIFI_PHY_11B;
        case 3:  return LLTD_WIFI_PHY_11G;
        case 4:  return LLTD_WIFI_PHY_11N;
        case 5:  return LLTD_WIFI_PHY_11AC;
        case 6:  return LLTD_WIFI_PHY_11AX;
        default: return LLTD_WIFI_PHY_NONE;
    }
}

const char *lltd_wifi_phy_mode_name(int phy_mode) {
    switch (phy_mode) {
        case LLTD_WIFI_PHY_NONE:  return "none";
        case LLTD_WIFI_PHY_11A:   return "802.11a";
        case LLTD_WIFI_PHY_11B:   return "802.11b";
        case LLTD_WIFI_PHY_11G:   return "802.11g";
        case LLTD_WIFI_PHY_11N:   return "802.11n";
        case LLTD_WIFI_PHY_11AC:  return "802.11ac";
        case LLTD_WIFI_PHY_11AX:  return "802.11ax";
        default:                  return "unknown";
    }
}

int lltd_darwin_wifi_get_metrics(const char *ifname, lltd_wifi_metrics_t *out) {
    int found;

    /* Validate arguments */
    if (!ifname || !out) {
        return -1;
    }

    /* Zero-initialize output */
    memset(out, 0, sizeof(lltd_wifi_metrics_t));

    /* Check if interface exists and is associated */
    if (!isAssociated(ifname)) {
        return -3;
    }

    /* Get RSSI */
    out->rssi_dbm = getIntProperty(ifname, kIO80211RSSI, &found);
    if (found) {
        out->flags |= LLTD_WIFI_FLAG_VALID_RSSI;
    }

    /* Get Noise */
    out->noise_dbm = getIntProperty(ifname, kIO80211Noise, &found);
    if (found) {
        out->flags |= LLTD_WIFI_FLAG_VALID_NOISE;
    }

    /* Get PHY mode */
    int rawPhyMode = getIntProperty(ifname, kIO80211PHYMode, &found);
    if (found) {
        out->phy_mode = mapPhyMode(rawPhyMode);
        if (out->phy_mode != LLTD_WIFI_PHY_NONE) {
            out->flags |= LLTD_WIFI_FLAG_VALID_PHY;
        }
    }

    /* Get channel width */
    out->chan_width_mhz = getIntProperty(ifname, kIO80211ChannelWidth, &found);
    if (found && out->chan_width_mhz > 0) {
        out->flags |= LLTD_WIFI_FLAG_VALID_WIDTH;
    }

    /* Get link speed - try multiple property names */
    out->link_mbps = getIntProperty(ifname, kIO80211LinkSpeed, &found);
    if (!found) {
        out->link_mbps = getIntProperty(ifname, kIO80211TxRate, &found);
    }
    if (found && out->link_mbps > 0) {
        out->flags |= LLTD_WIFI_FLAG_VALID_LINK;
    } else {
        /* Cannot proceed without link rate */
        return -3;
    }

    /* Compute max link speed based on PHY mode */
    int phy_family = lltd_wifi_phy_family_from_cwmode(out->phy_mode);
    int width = out->chan_width_mhz > 0 ? out->chan_width_mhz : 20;

    if (phy_family == WIFI_PHY_LEGACY_B) {
        /* 802.11b max is 11 Mbps */
        out->max_link_mbps = 11;
        out->flags |= LLTD_WIFI_FLAG_VALID_MAX;
    } else if (phy_family == WIFI_PHY_LEGACY_AG) {
        /* 802.11a/g max is 54 Mbps */
        out->max_link_mbps = 54;
        out->flags |= LLTD_WIFI_FLAG_VALID_MAX;
    } else if (phy_family != 0) {
        /* HT/VHT/HE: try to infer NSS/MCS/GI from observed rate */
        wifi_rate_params_t inferred;
        int tolerance = 15; /* Allow 15 Mbps tolerance for rate matching */

        if (lltd_wifi_infer_params(phy_family, width, out->link_mbps, tolerance, &inferred) == 0) {
            /* Successfully inferred parameters */
            out->nss_inferred = inferred.nss;
            out->mcs_inferred = inferred.mcs;
            out->gi_ns_inferred = inferred.gi_ns;
            out->flags |= LLTD_WIFI_FLAG_VALID_INFER;

            /* Calculate max rate using inferred NSS */
            out->max_link_mbps = lltd_wifi_calc_max_rate(phy_family, width, inferred.nss);
            out->flags |= LLTD_WIFI_FLAG_VALID_MAX;
        } else {
            /* Could not infer, assume 1 SS and mark as approximate */
            out->nss_inferred = 1;
            out->max_link_mbps = lltd_wifi_calc_max_rate(phy_family, width, 1);
            out->flags |= LLTD_WIFI_FLAG_VALID_MAX | LLTD_WIFI_FLAG_APPROX_MAX;
        }
    } else {
        /* Unknown PHY, use link rate as max */
        out->max_link_mbps = out->link_mbps;
        out->flags |= LLTD_WIFI_FLAG_VALID_MAX | LLTD_WIFI_FLAG_APPROX_MAX;
    }

    return 0;
}

#endif /* __APPLE__ */
