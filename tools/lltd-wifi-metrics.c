/******************************************************************************
 *                                                                            *
 *   lltd-wifi-metrics.c                                                      *
 *   LLTD Wi-Fi Metrics Test Tool                                             *
 *                                                                            *
 *   Command-line tool for testing Wi-Fi metrics retrieval via CoreWLAN.      *
 *   Darwin-only (macOS).                                                     *
 *                                                                            *
 *   Build:                                                                   *
 *     clang -o lltd-wifi-metrics lltd-wifi-metrics.c                         *
 *           ../lltdDaemon/darwin/lltd_wifi_corewlan.m                        *
 *           ../lltdDaemon/darwin/lltd_wifi_rates.c                           *
 *           -framework Foundation -framework CoreWLAN                        *
 *                                                                            *
 *   Usage:                                                                   *
 *     ./lltd-wifi-metrics [interface]                                        *
 *     ./lltd-wifi-metrics en0                                                *
 *                                                                            *
 *   Expected output on a Wi-Fi 6E link:                                      *
 *     Interface: en0                                                         *
 *     PHY Mode:  802.11ax (6)                                                *
 *     Channel:   160 MHz                                                     *
 *     RSSI:      -42 dBm                                                     *
 *     Noise:     -95 dBm                                                     *
 *     Link:      2401 Mbps                                                   *
 *     Max:       2402 Mbps                                                   *
 *     Inferred:  NSS=2, MCS=11, GI=800ns                                     *
 *     Flags:     0x00FF                                                      *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 07.01.2026.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include "../lltdDaemon/darwin/lltd_wifi_corewlan.h"
#endif

static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [interface]\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Query Wi-Fi metrics for the specified interface.\n");
    fprintf(stderr, "If no interface is specified, defaults to 'en0'.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s           # Query en0\n", progname);
    fprintf(stderr, "  %s en1       # Query en1\n", progname);
}

static void print_flags(int flags) {
    printf("Flags:     0x%04X (", flags);
    int first = 1;

    if (flags & LLTD_WIFI_FLAG_VALID_RSSI) {
        printf("%sRSSI", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_VALID_NOISE) {
        printf("%sNOISE", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_VALID_LINK) {
        printf("%sLINK", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_VALID_MAX) {
        printf("%sMAX", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_APPROX_MAX) {
        printf("%sAPPROX", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_VALID_PHY) {
        printf("%sPHY", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_VALID_WIDTH) {
        printf("%sWIDTH", first ? "" : ",");
        first = 0;
    }
    if (flags & LLTD_WIFI_FLAG_VALID_INFER) {
        printf("%sINFER", first ? "" : ",");
        first = 0;
    }

    printf(")\n");
}

#ifdef __APPLE__

int main(int argc, char *argv[]) {
    const char *ifname = "en0";
    lltd_wifi_metrics_t metrics;
    int result;

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        ifname = argv[1];
    }

    result = lltd_darwin_wifi_get_metrics(ifname, &metrics);

    if (result < 0) {
        switch (result) {
            case -1:
                fprintf(stderr, "Error: Invalid arguments\n");
                break;
            case -2:
                fprintf(stderr, "Error: Interface '%s' not found or not a Wi-Fi interface\n", ifname);
                break;
            case -3:
                fprintf(stderr, "Error: Interface '%s' is not associated to a network\n", ifname);
                break;
            default:
                fprintf(stderr, "Error: Unknown error %d\n", result);
                break;
        }
        return 1;
    }

    printf("\n");
    printf("=== Wi-Fi Metrics for %s ===\n", ifname);
    printf("\n");

    /* PHY Mode */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_PHY) {
        printf("PHY Mode:  %s (%d)\n", lltd_wifi_phy_mode_name(metrics.phy_mode), metrics.phy_mode);
    } else {
        printf("PHY Mode:  unknown\n");
    }

    /* Channel Width */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_WIDTH) {
        printf("Channel:   %d MHz\n", metrics.chan_width_mhz);
    } else {
        printf("Channel:   unknown\n");
    }

    /* RSSI */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_RSSI) {
        printf("RSSI:      %d dBm\n", metrics.rssi_dbm);
    } else {
        printf("RSSI:      unavailable\n");
    }

    /* Noise */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_NOISE) {
        printf("Noise:     %d dBm\n", metrics.noise_dbm);
    } else {
        printf("Noise:     unavailable\n");
    }

    /* Link Speed */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_LINK) {
        printf("Link:      %d Mbps\n", metrics.link_mbps);
    } else {
        printf("Link:      unavailable\n");
    }

    /* Max Link Speed */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_MAX) {
        printf("Max:       %d Mbps%s\n", metrics.max_link_mbps,
               (metrics.flags & LLTD_WIFI_FLAG_APPROX_MAX) ? " (approximate)" : "");
    } else {
        printf("Max:       unavailable\n");
    }

    /* Inferred Parameters */
    if (metrics.flags & LLTD_WIFI_FLAG_VALID_INFER) {
        printf("Inferred:  NSS=%d, MCS=%d, GI=%dns\n",
               metrics.nss_inferred, metrics.mcs_inferred, metrics.gi_ns_inferred);
    } else if (metrics.nss_inferred > 0) {
        printf("Inferred:  NSS=%d (assumed, could not match rate)\n", metrics.nss_inferred);
    } else {
        printf("Inferred:  none\n");
    }

    /* Flags */
    printf("\n");
    print_flags(metrics.flags);

    printf("\n");

    return 0;
}

#else /* !__APPLE__ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    fprintf(stderr, "Error: This tool is only available on macOS (Darwin)\n");
    return 1;
}

#endif /* __APPLE__ */
