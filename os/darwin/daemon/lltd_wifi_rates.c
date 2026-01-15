/******************************************************************************
 *                                                                            *
 *   lltd_wifi_rates.c                                                        *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Wi-Fi rate calculation tables and formulas.                              *
 *   Pure C89 for portability. No external dependencies.                      *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 07.01.2026.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#include "lltd_wifi_rates.h"
#include "lltd_wifi_corewlan.h"
#include <stdlib.h>

/*
 * Rate calculation is based on IEEE 802.11 standards.
 *
 * For OFDM-based PHYs (HT/VHT/HE), the formula is:
 *   Rate = N_SD * N_BPSCS * R * N_SS / T_SYM
 *
 * Where:
 *   N_SD    = number of data subcarriers
 *   N_BPSCS = bits per subcarrier per symbol (depends on modulation)
 *   R       = coding rate
 *   N_SS    = number of spatial streams
 *   T_SYM   = symbol duration (depends on GI)
 *
 * For simplicity and accuracy, we use pre-computed rate tables for
 * common configurations based on the 802.11 specifications.
 */

/*
 * HT (802.11n) data rates in Mbps * 10 (to avoid floats)
 * Index by [mcs][width_index][gi_index]
 * width_index: 0=20MHz, 1=40MHz
 * gi_index: 0=800ns (long), 1=400ns (short)
 */
static const int ht_rates_x10[8][2][2] = {
    /* MCS 0 (BPSK 1/2) */
    { { 65, 72 }, { 135, 150 } },
    /* MCS 1 (QPSK 1/2) */
    { { 130, 144 }, { 270, 300 } },
    /* MCS 2 (QPSK 3/4) */
    { { 195, 217 }, { 405, 450 } },
    /* MCS 3 (16-QAM 1/2) */
    { { 260, 289 }, { 540, 600 } },
    /* MCS 4 (16-QAM 3/4) */
    { { 390, 433 }, { 810, 900 } },
    /* MCS 5 (64-QAM 2/3) */
    { { 520, 578 }, { 1080, 1200 } },
    /* MCS 6 (64-QAM 3/4) */
    { { 585, 650 }, { 1215, 1350 } },
    /* MCS 7 (64-QAM 5/6) */
    { { 650, 722 }, { 1350, 1500 } }
};

/*
 * VHT (802.11ac) data rates in Mbps * 10 for 1 spatial stream
 * Index by [mcs][width_index][gi_index]
 * width_index: 0=20MHz, 1=40MHz, 2=80MHz, 3=160MHz
 * gi_index: 0=800ns (long), 1=400ns (short)
 */
static const int vht_rates_1ss_x10[10][4][2] = {
    /* MCS 0 (BPSK 1/2) */
    { { 65, 72 }, { 135, 150 }, { 293, 325 }, { 585, 650 } },
    /* MCS 1 (QPSK 1/2) */
    { { 130, 144 }, { 270, 300 }, { 585, 650 }, { 1170, 1300 } },
    /* MCS 2 (QPSK 3/4) */
    { { 195, 217 }, { 405, 450 }, { 878, 975 }, { 1755, 1950 } },
    /* MCS 3 (16-QAM 1/2) */
    { { 260, 289 }, { 540, 600 }, { 1170, 1300 }, { 2340, 2600 } },
    /* MCS 4 (16-QAM 3/4) */
    { { 390, 433 }, { 810, 900 }, { 1755, 1950 }, { 3510, 3900 } },
    /* MCS 5 (64-QAM 2/3) */
    { { 520, 578 }, { 1080, 1200 }, { 2340, 2600 }, { 4680, 5200 } },
    /* MCS 6 (64-QAM 3/4) */
    { { 585, 650 }, { 1215, 1350 }, { 2633, 2925 }, { 5265, 5850 } },
    /* MCS 7 (64-QAM 5/6) */
    { { 650, 722 }, { 1350, 1500 }, { 2925, 3250 }, { 5850, 6500 } },
    /* MCS 8 (256-QAM 3/4) */
    { { 780, 867 }, { 1620, 1800 }, { 3510, 3900 }, { 7020, 7800 } },
    /* MCS 9 (256-QAM 5/6) */
    { { 0, 0 }, { 1800, 2000 }, { 3900, 4333 }, { 7800, 8667 } }
    /* Note: MCS 9 at 20MHz is not valid for VHT */
};

/*
 * HE (802.11ax) data rates in Mbps * 10 for 1 spatial stream
 * Index by [mcs][width_index][gi_index]
 * width_index: 0=20MHz, 1=40MHz, 2=80MHz, 3=160MHz
 * gi_index: 0=800ns, 1=1600ns, 2=3200ns
 *
 * For HE, shorter GI = 800ns gives highest rate.
 * Values calculated using HE OFDMA parameters:
 *   20MHz: 234 data tones, 40MHz: 468, 80MHz: 980, 160MHz: 1960
 *   Symbol time: 12.8us + GI
 */
static const int he_rates_1ss_x10[12][4][3] = {
    /* MCS 0 (BPSK 1/2) */
    { { 86, 81, 73 }, { 172, 163, 146 }, { 360, 340, 306 }, { 721, 681, 613 } },
    /* MCS 1 (QPSK 1/2) */
    { { 172, 163, 146 }, { 344, 325, 293 }, { 721, 681, 613 }, { 1441, 1361, 1225 } },
    /* MCS 2 (QPSK 3/4) */
    { { 258, 244, 219 }, { 516, 488, 439 }, { 1081, 1021, 919 }, { 2162, 2042, 1838 } },
    /* MCS 3 (16-QAM 1/2) */
    { { 344, 325, 293 }, { 688, 650, 585 }, { 1441, 1361, 1225 }, { 2882, 2722, 2450 } },
    /* MCS 4 (16-QAM 3/4) */
    { { 516, 488, 439 }, { 1032, 975, 878 }, { 2162, 2042, 1838 }, { 4324, 4083, 3675 } },
    /* MCS 5 (64-QAM 2/3) */
    { { 688, 650, 585 }, { 1376, 1300, 1170 }, { 2882, 2722, 2450 }, { 5765, 5444, 4900 } },
    /* MCS 6 (64-QAM 3/4) */
    { { 774, 731, 658 }, { 1549, 1463, 1316 }, { 3243, 3063, 2756 }, { 6485, 6125, 5513 } },
    /* MCS 7 (64-QAM 5/6) */
    { { 860, 813, 731 }, { 1721, 1625, 1463 }, { 3603, 3403, 3063 }, { 7206, 6806, 6125 } },
    /* MCS 8 (256-QAM 3/4) */
    { { 1032, 975, 878 }, { 2065, 1950, 1755 }, { 4324, 4083, 3675 }, { 8647, 8167, 7350 } },
    /* MCS 9 (256-QAM 5/6) */
    { { 1147, 1083, 975 }, { 2294, 2167, 1950 }, { 4804, 4537, 4083 }, { 9608, 9074, 8167 } },
    /* MCS 10 (1024-QAM 3/4) */
    { { 1290, 1219, 1097 }, { 2581, 2438, 2194 }, { 5404, 5104, 4594 }, { 10809, 10208, 9188 } },
    /* MCS 11 (1024-QAM 5/6) */
    { { 1434, 1354, 1219 }, { 2868, 2708, 2438 }, { 6005, 5671, 5104 }, { 12010, 11342, 10208 } }
};

static int width_to_index(int width_mhz, int max_index)
{
    int idx;
    switch (width_mhz) {
        case 20:  idx = 0; break;
        case 40:  idx = 1; break;
        case 80:  idx = 2; break;
        case 160: idx = 3; break;
        default:  return -1;
    }
    return (idx <= max_index) ? idx : -1;
}

static int gi_to_ht_vht_index(int gi_ns)
{
    switch (gi_ns) {
        case 800:  return 0;
        case 400:  return 1;
        default:   return -1;
    }
}

static int gi_to_he_index(int gi_ns)
{
    switch (gi_ns) {
        case 800:  return 0;
        case 1600: return 1;
        case 3200: return 2;
        default:   return -1;
    }
}

int lltd_wifi_calc_rate(int phy_family, int width_mhz, int nss, int mcs, int gi_ns)
{
    int width_idx, gi_idx, rate_x10;

    if (nss < 1 || nss > WIFI_MAX_NSS) {
        return 0;
    }

    switch (phy_family) {
        case WIFI_PHY_LEGACY_B:
            /* 802.11b: 1, 2, 5.5, 11 Mbps */
            return 11;

        case WIFI_PHY_LEGACY_AG:
            /* 802.11a/g: max 54 Mbps */
            return 54;

        case WIFI_PHY_HT:
            if (mcs < 0 || mcs > WIFI_HT_MAX_MCS) return 0;
            width_idx = width_to_index(width_mhz, 1);
            gi_idx = gi_to_ht_vht_index(gi_ns);
            if (width_idx < 0 || gi_idx < 0) return 0;
            rate_x10 = ht_rates_x10[mcs][width_idx][gi_idx] * nss;
            return (rate_x10 + 5) / 10;

        case WIFI_PHY_VHT:
            if (mcs < 0 || mcs > WIFI_VHT_MAX_MCS) return 0;
            width_idx = width_to_index(width_mhz, 3);
            gi_idx = gi_to_ht_vht_index(gi_ns);
            if (width_idx < 0 || gi_idx < 0) return 0;
            rate_x10 = vht_rates_1ss_x10[mcs][width_idx][gi_idx];
            if (rate_x10 == 0) return 0; /* Invalid combination */
            rate_x10 *= nss;
            return (rate_x10 + 5) / 10;

        case WIFI_PHY_HE:
            if (mcs < 0 || mcs > WIFI_HE_MAX_MCS) return 0;
            width_idx = width_to_index(width_mhz, 3);
            gi_idx = gi_to_he_index(gi_ns);
            if (width_idx < 0 || gi_idx < 0) return 0;
            rate_x10 = he_rates_1ss_x10[mcs][width_idx][gi_idx] * nss;
            return (rate_x10 + 5) / 10;

        default:
            return 0;
    }
}

int lltd_wifi_infer_params(int phy_family, int width_mhz, int observed_mbps,
                           int tolerance_mbps, wifi_rate_params_t *out)
{
    int max_mcs, nss, mcs, gi_idx, rate, diff;
    int best_diff, best_nss, best_mcs, best_gi, best_rate;
    int gi_values_ht_vht[2];
    int gi_values_he[3];
    int *gi_values;
    int gi_count;

    if (!out) return -1;
    out->nss = 0;
    out->mcs = 0;
    out->gi_ns = 0;
    out->rate_mbps = 0;

    /* Determine MCS range and GI options based on PHY */
    switch (phy_family) {
        case WIFI_PHY_HT:
            max_mcs = WIFI_HT_MAX_MCS;
            gi_values_ht_vht[0] = 800;
            gi_values_ht_vht[1] = 400;
            gi_values = gi_values_ht_vht;
            gi_count = 2;
            break;

        case WIFI_PHY_VHT:
            max_mcs = WIFI_VHT_MAX_MCS;
            gi_values_ht_vht[0] = 800;
            gi_values_ht_vht[1] = 400;
            gi_values = gi_values_ht_vht;
            gi_count = 2;
            break;

        case WIFI_PHY_HE:
            max_mcs = WIFI_HE_MAX_MCS;
            gi_values_he[0] = 800;
            gi_values_he[1] = 1600;
            gi_values_he[2] = 3200;
            gi_values = gi_values_he;
            gi_count = 3;
            break;

        default:
            /* Legacy PHYs don't have MCS/NSS/GI to infer */
            return -1;
    }

    best_diff = tolerance_mbps + 1;
    best_nss = 0;
    best_mcs = 0;
    best_gi = 0;
    best_rate = 0;

    /* Search through all valid combinations */
    for (nss = 1; nss <= WIFI_MAX_NSS; nss++) {
        for (mcs = 0; mcs <= max_mcs; mcs++) {
            for (gi_idx = 0; gi_idx < gi_count; gi_idx++) {
                rate = lltd_wifi_calc_rate(phy_family, width_mhz, nss, mcs, gi_values[gi_idx]);
                if (rate == 0) continue;

                diff = observed_mbps - rate;
                if (diff < 0) diff = -diff;

                if (diff < best_diff) {
                    best_diff = diff;
                    best_nss = nss;
                    best_mcs = mcs;
                    best_gi = gi_values[gi_idx];
                    best_rate = rate;
                }
            }
        }
    }

    if (best_diff <= tolerance_mbps) {
        out->nss = best_nss;
        out->mcs = best_mcs;
        out->gi_ns = best_gi;
        out->rate_mbps = best_rate;
        return 0;
    }

    return -1;
}

int lltd_wifi_calc_max_rate(int phy_family, int width_mhz, int nss)
{
    int max_mcs, best_gi;

    switch (phy_family) {
        case WIFI_PHY_LEGACY_B:
            return 11;

        case WIFI_PHY_LEGACY_AG:
            return 54;

        case WIFI_PHY_HT:
            max_mcs = WIFI_HT_MAX_MCS;
            best_gi = 400; /* Short GI */
            break;

        case WIFI_PHY_VHT:
            max_mcs = WIFI_VHT_MAX_MCS;
            best_gi = 400; /* Short GI */
            break;

        case WIFI_PHY_HE:
            max_mcs = WIFI_HE_MAX_MCS;
            best_gi = 800; /* 800ns gives highest rate for HE */
            break;

        default:
            return 0;
    }

    return lltd_wifi_calc_rate(phy_family, width_mhz, nss, max_mcs, best_gi);
}

int lltd_wifi_phy_family_from_cwmode(int cw_phy_mode)
{
    switch (cw_phy_mode) {
        case LLTD_WIFI_PHY_11B:
            return WIFI_PHY_LEGACY_B;
        case LLTD_WIFI_PHY_11A:
        case LLTD_WIFI_PHY_11G:
            return WIFI_PHY_LEGACY_AG;
        case LLTD_WIFI_PHY_11N:
            return WIFI_PHY_HT;
        case LLTD_WIFI_PHY_11AC:
            return WIFI_PHY_VHT;
        case LLTD_WIFI_PHY_11AX:
            return WIFI_PHY_HE;
        default:
            return 0;
    }
}
