/******************************************************************************
 *                                                                            *
 *   freebsd-ops.c                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid.h>

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
    uuid_t uuid;
    uint32_t status = 0;
    uuid_create(&uuid, &status);
    if (status != uuid_s_ok) {
        *pointer = NULL;
        return;
    }
    *pointer = malloc(sizeof(uuid));
    if (!*pointer) {
        return;
    }
    memcpy(*pointer, &uuid, sizeof(uuid));
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
    network_interface_t *iface = networkInterface;
    if (!iface || !iface->deviceName) {
        return;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface->deviceName, sizeof(ifr.ifr_name) - 1);
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        if (set) {
            ifr.ifr_flags |= IFF_PROMISC;
        } else {
            ifr.ifr_flags &= ~IFF_PROMISC;
        }
        ioctl(sock, SIOCSIFFLAGS, &ifr);
    }
    close(sock);
}
