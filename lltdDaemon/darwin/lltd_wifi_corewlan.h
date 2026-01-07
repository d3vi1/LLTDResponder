/******************************************************************************
 *                                                                            *
 *   lltd_wifi_corewlan.h                                                     *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Pure C API for Darwin Wi-Fi metrics via CoreWLAN.                        *
 *   This header is safe to include from C89 code.                            *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 07.01.2026.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTD_WIFI_COREWLAN_H
#define LLTD_WIFI_COREWLAN_H

#ifdef __APPLE__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flags bitmask for lltd_wifi_metrics_t.flags
 */
#define LLTD_WIFI_FLAG_VALID_RSSI       (1 << 0)
#define LLTD_WIFI_FLAG_VALID_NOISE      (1 << 1)
#define LLTD_WIFI_FLAG_VALID_LINK       (1 << 2)
#define LLTD_WIFI_FLAG_VALID_MAX        (1 << 3)
#define LLTD_WIFI_FLAG_APPROX_MAX       (1 << 4)  /* Max is approximate (NSS inferred) */
#define LLTD_WIFI_FLAG_VALID_PHY        (1 << 5)
#define LLTD_WIFI_FLAG_VALID_WIDTH      (1 << 6)
#define LLTD_WIFI_FLAG_VALID_INFER      (1 << 7)  /* NSS/MCS/GI were successfully inferred */

/*
 * PHY mode constants (matching CWPHYMode values)
 */
#define LLTD_WIFI_PHY_NONE          0
#define LLTD_WIFI_PHY_11A           1
#define LLTD_WIFI_PHY_11B           2
#define LLTD_WIFI_PHY_11G           3
#define LLTD_WIFI_PHY_11N           4
#define LLTD_WIFI_PHY_11AC          5
#define LLTD_WIFI_PHY_11AX          6

/*
 * Wi-Fi metrics structure
 *
 * All rate values are in Mbps (integer).
 * RSSI/noise are in dBm.
 */
typedef struct lltd_wifi_metrics_s {
    int rssi_dbm;           /* e.g., -42 */
    int noise_dbm;          /* optional if available, else 0 */
    int link_mbps;          /* current tx rate Mbps (integer) */
    int max_link_mbps;      /* computed theoretical max Mbps */
    int phy_mode;           /* numeric CWPHYMode value for logging */
    int chan_width_mhz;     /* 20/40/80/160 (0 if unknown) */
    int nss_inferred;       /* inferred spatial streams count (0 if unknown) */
    int mcs_inferred;       /* inferred MCS (0 if unknown) */
    int gi_ns_inferred;     /* inferred GI in ns (0 if unknown) */
    int flags;              /* bitmask of LLTD_WIFI_FLAG_* values */
} lltd_wifi_metrics_t;

/*
 * Query Wi-Fi metrics for the given interface.
 *
 * Parameters:
 *   ifname - BSD interface name (e.g., "en0")
 *   out    - pointer to metrics structure (will be zero-initialized)
 *
 * Returns:
 *    0  - success (check flags for which fields are valid)
 *   -1  - invalid arguments
 *   -2  - interface not found
 *   -3  - not Wi-Fi / not associated / link speed unavailable
 */
int lltd_darwin_wifi_get_metrics(const char *ifname, lltd_wifi_metrics_t *out);

/*
 * Get a human-readable PHY mode name.
 *
 * Parameters:
 *   phy_mode - one of LLTD_WIFI_PHY_* constants
 *
 * Returns:
 *   Static string (e.g., "802.11ax", "802.11ac", etc.)
 */
const char *lltd_wifi_phy_mode_name(int phy_mode);

#ifdef __cplusplus
}
#endif

#endif /* __APPLE__ */

#endif /* LLTD_WIFI_COREWLAN_H */
