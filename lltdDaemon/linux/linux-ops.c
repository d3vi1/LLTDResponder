/******************************************************************************
 *                                                                            *
 *   linux-ops.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Linux platform hooks used by the protocol logic (TLVs, etc.).            *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"

#include <stdlib.h>
#include <string.h>

static void lltd_dup_string(const char *value, char **out, size_t *out_size) {
    if (!out || !out_size) {
        return;
    }
    if (!value) {
        *out = NULL;
        *out_size = 0;
        return;
    }
    size_t len = strlen(value);
    char *copy = malloc(len + 1);
    if (!copy) {
        *out = NULL;
        *out_size = 0;
        return;
    }
    memcpy(copy, value, len + 1);
    *out = copy;
    *out_size = len;
}

void getMachineName(char **data, size_t *stringSize) {
    lltd_dup_string("lltd-linux", data, stringSize);
}

void getFriendlyName(char **data, size_t *stringSize) {
    lltd_dup_string("LLTD Responder", data, stringSize);
}

void getSupportInfo(void **data, size_t *stringSize) {
    if (!data || !stringSize) {
        return;
    }
    char *out = NULL;
    size_t out_len = 0;
    lltd_dup_string("https://example.invalid/support", &out, &out_len);
    *data = out;
    *stringSize = out_len;
}

void getIconImage(void **icon, size_t *iconsize) {
    if (icon) {
        *icon = NULL;
    }
    if (iconsize) {
        *iconsize = 0;
    }
}

void getUpnpUuid(void **data) {
    if (!data) {
        return;
    }
    uint8_t *buffer = calloc(1, 16);
    *data = buffer;
}

void getHwId(void *data) {
    if (!data) {
        return;
    }
    memset(data, 0, 64);
}

void getDetailedIconImage(void **data, size_t *iconsize) {
    if (data) {
        *data = NULL;
    }
    if (iconsize) {
        *iconsize = 0;
    }
}

void getHostCharacteristics(void *data) {
    (void)data;
}

void getComponentTable(void **data, size_t *dataSize) {
    if (data) {
        *data = NULL;
    }
    if (dataSize) {
        *dataSize = 0;
    }
}

void getPerformanceCounterFrequency(void *data) {
    (void)data;
}

void setPromiscuous(void *currentNetworkInterface, boolean_t set) {
    (void)currentNetworkInterface;
    (void)set;
}

boolean_t getWifiMode(void *currentNetworkInterface) {
    (void)currentNetworkInterface;
    return false;
}

boolean_t getBSSID(void **data, void *currentNetworkInterface) {
    (void)currentNetworkInterface;
    if (data) {
        *data = NULL;
    }
    return false;
}

boolean_t getSSID(char **ssid, size_t *size, void *currentNetworkInterface) {
    (void)currentNetworkInterface;
    if (ssid) {
        *ssid = NULL;
    }
    if (size) {
        *size = 0;
    }
    return false;
}

boolean_t getWifiMaxRate(uint32_t *rateMbps, void *currentNetworkInterface) {
    (void)currentNetworkInterface;
    if (rateMbps) {
        *rateMbps = 0;
    }
    return false;
}

boolean_t getWifiRssi(int8_t *rssi, void *currentNetworkInterface) {
    (void)currentNetworkInterface;
    if (rssi) {
        *rssi = 0;
    }
    return false;
}

boolean_t getWifiPhyMedium(uint32_t *phyMedium, void *currentNetworkInterface) {
    (void)currentNetworkInterface;
    if (phyMedium) {
        *phyMedium = 0;
    }
    return false;
}

