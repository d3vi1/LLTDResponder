#include "lltdTestShim.h"

#include <stdlib.h>

void getMachineName(char **name, size_t *size) {
    const char *value = "lltd-test-host";
    size_t len = strlen(value);
    char *copy = malloc(len + 1);
    if (!copy) {
        *name = NULL;
        *size = 0;
        return;
    }
    memcpy(copy, value, len + 1);
    *name = copy;
    *size = len;
}

void getSupportInfo(void **info, size_t *size) {
    const char *value = "https://example.invalid/support";
    size_t len = strlen(value);
    char *copy = malloc(len + 1);
    if (!copy) {
        *info = NULL;
        *size = 0;
        return;
    }
    memcpy(copy, value, len + 1);
    *info = copy;
    *size = len;
}

void getUpnpUuid(void **uuid) {
    uint8_t *buffer = calloc(1, 16);
    *uuid = buffer;
}

uint8_t getWifiMode(void *networkInterface) {
    (void)networkInterface;
    return 0;
}

bool getBSSID(void *bssid, void *networkInterface) {
    (void)networkInterface;
    if (!bssid) {
        return false;
    }
    memset(bssid, 0, 6);
    return true;
}

bool getSSID(char **ssid, size_t *size, void *networkInterface) {
    (void)networkInterface;
    *ssid = NULL;
    *size = 0;
    return false;
}

bool getWifiMaxRate(uint32_t *rate, void *networkInterface) {
    (void)networkInterface;
    if (rate) {
        *rate = 0;
    }
    return false;
}

bool getWifiRssi(int8_t *rssi, void *networkInterface) {
    (void)networkInterface;
    if (rssi) {
        *rssi = 0;
    }
    return false;
}

bool getWifiPhyMedium(uint32_t *medium, void *networkInterface) {
    (void)networkInterface;
    if (medium) {
        *medium = 0;
    }
    return false;
}
