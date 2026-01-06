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
    // Set a test UUID pattern
    if (buffer) {
        buffer[0] = 0x12;
        buffer[1] = 0x34;
        buffer[2] = 0x56;
        buffer[3] = 0x78;
        buffer[4] = 0x9a;
        buffer[5] = 0xbc;
        buffer[6] = 0xde;
        buffer[7] = 0xf0;
        buffer[8] = 0x11;
        buffer[9] = 0x22;
        buffer[10] = 0x33;
        buffer[11] = 0x44;
        buffer[12] = 0x55;
        buffer[13] = 0x66;
        buffer[14] = 0x77;
        buffer[15] = 0x88;
    }
    *uuid = buffer;
}

void getHwId(void *data) {
    if (!data) {
        return;
    }
    // Return empty hardware ID for testing
    memset(data, 0, 64);
}

void getDetailedIconImage(void **data, size_t *iconsize) {
    // No detailed icon in test mode
    *data = NULL;
    *iconsize = 0;
}

void getComponentTable(void **data, size_t *dataSize) {
    // No component table in test mode
    *data = NULL;
    *dataSize = 0;
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
