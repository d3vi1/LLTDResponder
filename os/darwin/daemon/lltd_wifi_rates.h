/******************************************************************************
 *                                                                            *
 *   lltd_wifi_rates.h                                                        *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Wi-Fi rate calculation tables and formulas.                              *
 *   Pure C89 for portability. No external dependencies.                      *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 07.01.2026.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTD_WIFI_RATES_H
#define LLTD_WIFI_RATES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PHY family constants (used internally)
 */
#define WIFI_PHY_LEGACY_B   1
#define WIFI_PHY_LEGACY_AG  2
#define WIFI_PHY_HT         3   /* 802.11n */
#define WIFI_PHY_VHT        4   /* 802.11ac */
#define WIFI_PHY_HE         5   /* 802.11ax */

/*
 * Guard interval values in nanoseconds
 */
#define WIFI_GI_LONG_NS     800     /* Standard GI for HT/VHT, also HE normal */
#define WIFI_GI_SHORT_NS    400     /* Short GI for HT/VHT */
#define WIFI_GI_HE_1600_NS  1600    /* HE extended GI */
#define WIFI_GI_HE_3200_NS  3200    /* HE long GI */

/*
 * Maximum MCS per PHY family
 */
#define WIFI_HT_MAX_MCS     7       /* 64-QAM 5/6 */
#define WIFI_VHT_MAX_MCS    9       /* 256-QAM 5/6 */
#define WIFI_HE_MAX_MCS     11      /* 1024-QAM 5/6 */

/*
 * Maximum spatial streams commonly supported
 */
#define WIFI_MAX_NSS        8

/*
 * Channel widths in MHz
 */
#define WIFI_WIDTH_20       20
#define WIFI_WIDTH_40       40
#define WIFI_WIDTH_80       80
#define WIFI_WIDTH_160      160

/*
 * Inferred rate parameters
 */
typedef struct wifi_rate_params_s {
    int nss;            /* 1-8 spatial streams */
    int mcs;            /* MCS index */
    int gi_ns;          /* Guard interval in nanoseconds */
    int rate_mbps;      /* Data rate in Mbps */
} wifi_rate_params_t;

/*
 * Calculate the data rate for given parameters.
 *
 * Parameters:
 *   phy_family   - WIFI_PHY_* constant
 *   width_mhz    - channel width (20/40/80/160)
 *   nss          - number of spatial streams (1-8)
 *   mcs          - MCS index (0-7 for HT, 0-9 for VHT, 0-11 for HE)
 *   gi_ns        - guard interval in nanoseconds
 *
 * Returns:
 *   Data rate in Mbps, or 0 if parameters are invalid.
 */
int lltd_wifi_calc_rate(int phy_family, int width_mhz, int nss, int mcs, int gi_ns);

/*
 * Infer NSS/MCS/GI from an observed link rate.
 *
 * Searches through valid rate combinations for the given PHY family
 * and channel width to find the best match for the observed rate.
 *
 * Parameters:
 *   phy_family     - WIFI_PHY_* constant
 *   width_mhz      - channel width (20/40/80/160)
 *   observed_mbps  - observed link rate in Mbps
 *   tolerance_mbps - acceptable tolerance for matching (e.g., 10)
 *   out            - pointer to store inferred parameters
 *
 * Returns:
 *   0  - match found (out is populated)
 *   -1 - no match within tolerance
 */
int lltd_wifi_infer_params(int phy_family, int width_mhz, int observed_mbps,
                           int tolerance_mbps, wifi_rate_params_t *out);

/*
 * Calculate maximum theoretical rate for given PHY and width.
 *
 * Uses the best MCS and shortest GI for the PHY family.
 *
 * Parameters:
 *   phy_family - WIFI_PHY_* constant
 *   width_mhz  - channel width (20/40/80/160)
 *   nss        - number of spatial streams
 *
 * Returns:
 *   Maximum data rate in Mbps, or 0 if invalid.
 */
int lltd_wifi_calc_max_rate(int phy_family, int width_mhz, int nss);

/*
 * Get the PHY family constant from a CWPHYMode value.
 *
 * Parameters:
 *   cw_phy_mode - CWPHYMode value (0-6)
 *
 * Returns:
 *   WIFI_PHY_* constant, or 0 if unknown.
 */
int lltd_wifi_phy_family_from_cwmode(int cw_phy_mode);

#ifdef __cplusplus
}
#endif

#endif /* LLTD_WIFI_RATES_H */
