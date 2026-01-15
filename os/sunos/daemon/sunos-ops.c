/******************************************************************************
 *                                                                            *
 *   sunos-ops.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void copyAsciiToUcs2le(const char *input, char **output, size_t *size) {
    if (!input) {
        *output = NULL;
        *size = 0;
        return;
    }
    size_t len = strlen(input);
    *size = len * 2;
    char *buffer = malloc(*size);
    if (!buffer) {
        *output = NULL;
        *size = 0;
        return;
    }
    for (size_t i = 0; i < len; i++) {
        buffer[i * 2] = input[i];
        buffer[i * 2 + 1] = 0x00;
    }
    *output = buffer;
}

void getUpnpUuid(void **pointer) {
    *pointer = NULL;
}

void getIconImage(void **icon, size_t *iconsize) {
    *icon = NULL;
    *iconsize = 0;
}

void getMachineName(char **pointer, size_t *stringSize) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        *pointer = NULL;
        *stringSize = 0;
        return;
    }
    copyAsciiToUcs2le(hostname, pointer, stringSize);
}

void getFriendlyName(char **pointer, size_t *stringSize) {
    getMachineName(pointer, stringSize);
}

void getSupportInfo(void **data, size_t *stringSize) {
    *data = NULL;
    *stringSize = 0;
}

boolean_t getWifiMode(void *networkInterface) {
    (void)networkInterface;
    return false;
}

boolean_t getBSSID(void **data, void *networkInterface) {
    (void)networkInterface;
    *data = NULL;
    return false;
}

void getHwId(void *data) {
    if (data) {
        memset(data, 0, 16);
    }
}

void getDetailedIconImage(void **data, size_t *iconsize) {
    *data = NULL;
    *iconsize = 0;
}

void getHostCharacteristics(void *data) {
    if (!data) {
        return;
    }
    uint16_t *characteristics = data;
    *characteristics = htons(Config_TLV_HasManagementURL_Value);
}

void getComponentTable(void **data, size_t *dataSize) {
    *data = NULL;
    *dataSize = 0;
}

void setPromiscuous(void *networkInterface, boolean_t set) {
    (void)networkInterface;
    (void)set;
}
