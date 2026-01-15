/******************************************************************************
 *                                                                            *
 *   lltd_wifi_corewlan.m                                                     *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Objective-C implementation of Wi-Fi metrics using CoreWLAN.              *
 *   Exposes a pure C API for use from C code.                                *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 07.01.2026.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#import <Foundation/Foundation.h>
#import <CoreWLAN/CoreWLAN.h>

#include "lltd_wifi_corewlan.h"
#include "lltd_wifi_rates.h"
#include <string.h>

/*
 * Map CWChannelWidth to MHz
 */
static int channelWidthToMHz(CWChannelWidth width) {
    switch (width) {
        case kCWChannelWidth20MHz:  return 20;
        case kCWChannelWidth40MHz:  return 40;
        case kCWChannelWidth80MHz:  return 80;
        case kCWChannelWidth160MHz: return 160;
        default:                    return 0;
    }
}

/*
 * Map CWPHYMode to our PHY mode constants
 */
static int phyModeToConstant(CWPHYMode mode) {
    switch (mode) {
        case kCWPHYMode11a:  return LLTD_WIFI_PHY_11A;
        case kCWPHYMode11b:  return LLTD_WIFI_PHY_11B;
        case kCWPHYMode11g:  return LLTD_WIFI_PHY_11G;
        case kCWPHYMode11n:  return LLTD_WIFI_PHY_11N;
        case kCWPHYMode11ac: return LLTD_WIFI_PHY_11AC;
        default:             break;
    }

    /* Check for 802.11ax (Wi-Fi 6) - may not be defined in older SDKs */
#if defined(kCWPHYMode11ax)
    if (mode == kCWPHYMode11ax) {
        return LLTD_WIFI_PHY_11AX;
    }
#endif

    /* For newer PHY modes on older SDKs, check numeric value */
    /* kCWPHYMode11ax = 6 on macOS 11+ */
    if ((int)mode == 6) {
        return LLTD_WIFI_PHY_11AX;
    }

    return LLTD_WIFI_PHY_NONE;
}

/*
 * Get the transmit rate from CWInterface.
 * Tries multiple selectors for compatibility across macOS versions.
 */
static double getTransmitRate(CWInterface *iface) {
    double rate = 0.0;

    /* Try transmitRate first (modern API) */
    if ([iface respondsToSelector:@selector(transmitRate)]) {
        rate = [iface transmitRate];
        if (rate > 0) return rate;
    }

    /* Try txRate (if it exists as alternate name) */
    SEL txRateSel = NSSelectorFromString(@"txRate");
    if ([iface respondsToSelector:txRateSel]) {
        NSMethodSignature *sig = [iface methodSignatureForSelector:txRateSel];
        if (sig && strcmp([sig methodReturnType], "d") == 0) {
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
            [inv setSelector:txRateSel];
            [inv invokeWithTarget:iface];
            [inv getReturnValue:&rate];
            if (rate > 0) return rate;
        }
    }

    return rate;
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
    /* Use old-style autorelease pool for compatibility with deployment target < 10.7 */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int result = 0;

    /* Validate arguments */
    if (!ifname || !out) {
        [pool drain];
        return -1;
    }

    /* Zero-initialize output */
    memset(out, 0, sizeof(lltd_wifi_metrics_t));

    /* Get the CWWiFiClient */
    CWWiFiClient *client = [CWWiFiClient sharedWiFiClient];
    if (!client) {
        [pool drain];
        return -2;
    }

    /* Get the interface by name */
    NSString *ifnameStr = [NSString stringWithUTF8String:ifname];
    CWInterface *iface = [client interfaceWithName:ifnameStr];
    if (!iface) {
        [pool drain];
        return -2;
    }

    /* Check if we're associated to a network */
    if (![iface ssid]) {
        [pool drain];
        return -3;
    }

    /* Get RSSI */
    if ([iface respondsToSelector:@selector(rssiValue)]) {
        out->rssi_dbm = (int)[iface rssiValue];
        out->flags |= LLTD_WIFI_FLAG_VALID_RSSI;
    }

    /* Get Noise */
    if ([iface respondsToSelector:@selector(noiseMeasurement)]) {
        out->noise_dbm = (int)[iface noiseMeasurement];
        out->flags |= LLTD_WIFI_FLAG_VALID_NOISE;
    }

    /* Get PHY mode */
    CWPHYMode phyMode = kCWPHYModeNone;
    if ([iface respondsToSelector:@selector(activePHYMode)]) {
        phyMode = [iface activePHYMode];
        out->phy_mode = phyModeToConstant(phyMode);
        if (out->phy_mode != LLTD_WIFI_PHY_NONE) {
            out->flags |= LLTD_WIFI_FLAG_VALID_PHY;
        }
    }

    /* Get channel width */
    CWChannel *channel = [iface wlanChannel];
    if (channel) {
        out->chan_width_mhz = channelWidthToMHz([channel channelWidth]);
        if (out->chan_width_mhz > 0) {
            out->flags |= LLTD_WIFI_FLAG_VALID_WIDTH;
        }
    }

    /* Get current link speed (transmit rate) */
    double txRate = getTransmitRate(iface);
    if (txRate > 0) {
        out->link_mbps = (int)(txRate + 0.5); /* Round to nearest integer */
        out->flags |= LLTD_WIFI_FLAG_VALID_LINK;
    } else {
        /* Cannot proceed without link rate */
        [pool drain];
        return -3;
    }

    /* Now compute max link speed */
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

    [pool drain];
    return result;
}
