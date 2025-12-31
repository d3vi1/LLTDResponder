/******************************************************************************
 *                                                                            *
 *   linux-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 09.02.2016.                      *
 *   Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTD_USE_SYSTEMD
#define LLTD_USE_SYSTEMD 1
#endif

#include "../lltdDaemon.h"
#include "../lltdAutomata.h"
#include "../lltdBlock.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-id128.h>

#define NM_BUS_NAME "org.freedesktop.NetworkManager"
#define NM_PATH     "/org/freedesktop/NetworkManager"
#define NM_INTERFACE "org.freedesktop.NetworkManager"
#define NM_DEVICE_INTERFACE "org.freedesktop.NetworkManager.Device"
#define NM_ACTIVE_CONNECTION_INTERFACE "org.freedesktop.NetworkManager.Connection.Active"

#define NM_DEVICE_TYPE_ETHERNET 1
#define NM_DEVICE_TYPE_WIFI     2

static volatile sig_atomic_t exitFlag = 0;

typedef struct {
    char *hostname;
    char *iconName;
    char *uuid;
} linux_hostInfo_t;

typedef struct {
    network_interface_t iface;
    pthread_t thread;
    automata *mapping;
    automata *session;
    automata *enumeration;
    char *connectionUuid;
} linux_interface_ctx_t;

static linux_hostInfo_t hostInfo = {0};

static void freeAutomata(automata *autom) {
    if (!autom) {
        return;
    }
    free(autom->extra);
    free(autom);
}

static void SignalHandler(int signal) {
    log_debug("Interrupted by signal #%d", signal);
    exitFlag = 1;
}

static char *copyBusPropertyString(sd_bus *bus,
                                      const char *service,
                                      const char *path,
                                      const char *interface,
                                      const char *property) {
    char *value = NULL;
    int ret = sd_bus_get_property_string(bus, service, path, interface, property, NULL, &value);
    if (ret < 0) {
        return NULL;
    }
    return value;
}

static uint32_t getBusPropertyU32(sd_bus *bus,
                                     const char *service,
                                     const char *path,
                                     const char *interface,
                                     const char *property) {
    uint32_t value = 0;
    int ret = sd_bus_get_property_trivial(bus, service, path, interface, property, NULL, 'u', &value);
    if (ret < 0) {
        return 0;
    }
    return value;
}

static void loadHostInfo(sd_bus *bus) {
    if (hostInfo.hostname) {
        free(hostInfo.hostname);
        hostInfo.hostname = NULL;
    }
    if (hostInfo.iconName) {
        free(hostInfo.iconName);
        hostInfo.iconName = NULL;
    }
    if (hostInfo.uuid) {
        free(hostInfo.uuid);
        hostInfo.uuid = NULL;
    }

    char *pretty = copyBusPropertyString(bus, "org.freedesktop.hostname1", "/org/freedesktop/hostname1",
                                            "org.freedesktop.hostname1", "PrettyHostname");
    char *fallback = copyBusPropertyString(bus, "org.freedesktop.hostname1", "/org/freedesktop/hostname1",
                                              "org.freedesktop.hostname1", "Hostname");
    hostInfo.hostname = pretty ? pretty : fallback;
    if (pretty && fallback) {
        free(fallback);
    }

    hostInfo.iconName = copyBusPropertyString(bus, "org.freedesktop.hostname1", "/org/freedesktop/hostname1",
                                                   "org.freedesktop.hostname1", "IconName");

    sd_id128_t machine_id;
    if (sd_id128_get_machine(&machine_id) == 0) {
        hostInfo.uuid = malloc(SD_ID128_STRING_MAX);
        if (hostInfo.uuid) {
            sd_id128_to_string(machine_id, hostInfo.uuid);
        }
    }

    log_info("Systemd host info: hostname=%s icon=%s uuid=%s",
             hostInfo.hostname ? hostInfo.hostname : "(none)",
             hostInfo.iconName ? hostInfo.iconName : "(none)",
             hostInfo.uuid ? hostInfo.uuid : "(none)");
}

static bool fillInterfaceDetails(network_interface_t *iface, const char *ifname) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    iface->deviceName = strdup(ifname);
    iface->ifIndex = if_nametoindex(ifname);
    if (!iface->deviceName || iface->ifIndex == 0) {
        return false;
    }
    iface->interfaceType = NetworkInterfaceTypeEthernet;
    iface->MediumType = 0;
    iface->LinkSpeed = 0;
    iface->flags = 0;

    iface->socket = socket(AF_PACKET, SOCK_RAW, htons(lltdEtherType));
    if (iface->socket < 0) {
        log_err("Could not create packet socket on %s: %s", ifname, strerror(errno));
        return false;
    }

    memset(&iface->socketAddr, 0, sizeof(iface->socketAddr));
    iface->socketAddr.sll_family = AF_PACKET;
    iface->socketAddr.sll_protocol = htons(lltdEtherType);
    iface->socketAddr.sll_ifindex = iface->ifIndex;
    iface->socketAddr.sll_halen = ETH_ALEN;
    memset(iface->socketAddr.sll_addr, 0xFF, ETH_ALEN);

    if (bind(iface->socket, (struct sockaddr *)&iface->socketAddr, sizeof(iface->socketAddr)) < 0) {
        log_err("Could not bind packet socket on %s: %s", ifname, strerror(errno));
        close(iface->socket);
        return false;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    if (ioctl(iface->socket, SIOCGIFMTU, &ifr) == 0) {
        iface->MTU = ifr.ifr_mtu;
    } else {
        iface->MTU = 1500;
    }

    if (ioctl(iface->socket, SIOCGIFHWADDR, &ifr) == 0) {
        memcpy(iface->macAddress, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    }

    if (ioctl(iface->socket, SIOCGIFFLAGS, &ifr) == 0) {
        iface->flags = ifr.ifr_flags;
    }

    iface->recvBuffer = malloc(iface->MTU);
    if (!iface->recvBuffer) {
        log_err("Failed to allocate receive buffer for %s", ifname);
        close(iface->socket);
        return false;
    }
    iface->seeList = NULL;
    iface->seeListCount = 0;
    iface->MapperSeqNumber = 0;
    memset(iface->MapperHwAddress, 0, sizeof(iface->MapperHwAddress));

    log_notice("Bound LLTD socket on %s (ifindex=%d)", ifname, iface->ifIndex);
    return true;
}

static void *lltdLoop(void *data) {
    linux_interface_ctx_t *ctx = data;
    network_interface_t *iface = &ctx->iface;

    while (!exitFlag) {
        ssize_t bytes = recvfrom(iface->socket, iface->recvBuffer, iface->MTU, 0, NULL, NULL);
        if (bytes <= 0) {
            continue;
        }
        lltd_demultiplex_header_t *header = iface->recvBuffer;
        switch_state_mapping(ctx->mapping, header->opcode, "rx");
        switch_state_session(ctx->session, header->opcode, "rx");
        parseFrame(iface->recvBuffer, iface);
    }

    return NULL;
}

static char *getActiveConnectionUuid(sd_bus *bus, const char *device_path) {
    char *active_path = copyBusPropertyString(bus, NM_BUS_NAME, device_path, NM_DEVICE_INTERFACE, "ActiveConnection");
    if (!active_path || strcmp(active_path, "/") == 0) {
        if (active_path) {
            free(active_path);
        }
        return NULL;
    }

    char *uuid = copyBusPropertyString(bus, NM_BUS_NAME, active_path,
                                          NM_ACTIVE_CONNECTION_INTERFACE, "Uuid");
    free(active_path);
    return uuid;
}

static bool shouldBindDevice(sd_bus *bus, const char *device_path, char **ifname, char **uuid) {
    *ifname = copyBusPropertyString(bus, NM_BUS_NAME, device_path, NM_DEVICE_INTERFACE, "Interface");
    if (!*ifname || strlen(*ifname) == 0) {
        return false;
    }

    uint32_t device_type = getBusPropertyU32(bus, NM_BUS_NAME, device_path, NM_DEVICE_INTERFACE, "DeviceType");
    if (device_type != NM_DEVICE_TYPE_ETHERNET && device_type != NM_DEVICE_TYPE_WIFI) {
        return false;
    }

    *uuid = getActiveConnectionUuid(bus, device_path);
    return true;
}

static linux_interface_ctx_t *loadInterfaces(sd_bus *bus, size_t *count) {
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    linux_interface_ctx_t *interfaces = NULL;
    size_t interfaceCount = 0;

    int ret = sd_bus_call_method(bus, NM_BUS_NAME, NM_PATH, NM_INTERFACE, "GetDevices", &error, &reply, "");
    if (ret < 0) {
        log_err("Failed to read NetworkManager devices: %s", error.message ? error.message : "unknown");
        sd_bus_error_free(&error);
        return NULL;
    }

    ret = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "o");
    if (ret < 0) {
        sd_bus_message_unref(reply);
        return NULL;
    }

    const char *device_path = NULL;
    while ((ret = sd_bus_message_read(reply, "o", &device_path)) > 0) {
        if (!device_path) {
            continue;
        }

        char *ifname = NULL;
        char *uuid = NULL;
        if (!shouldBindDevice(bus, device_path, &ifname, &uuid)) {
            if (ifname) {
                free(ifname);
            }
            if (uuid) {
                free(uuid);
            }
            continue;
        }

        linux_interface_ctx_t *expanded = realloc(interfaces, sizeof(*interfaces) * (interfaceCount + 1));
        if (!expanded) {
            free(ifname);
            free(uuid);
            break;
        }
        interfaces = expanded;
        memset(&interfaces[interfaceCount], 0, sizeof(*interfaces));
        interfaces[interfaceCount].connectionUuid = uuid;
        if (!fillInterfaceDetails(&interfaces[interfaceCount].iface, ifname)) {
            free(interfaces[interfaceCount].iface.deviceName);
            free(ifname);
            free(uuid);
            continue;
        }
        interfaces[interfaceCount].mapping = init_automata_mapping();
        interfaces[interfaceCount].session = init_automata_session();
        interfaces[interfaceCount].enumeration = init_automata_enumeration();

        pthread_create(&interfaces[interfaceCount].thread, NULL, lltdLoop, &interfaces[interfaceCount]);
        log_info("NetworkManager device %s uuid=%s", ifname, uuid ? uuid : "(none)");
        interfaceCount++;
        free(ifname);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    *count = interfaceCount;
    return interfaces;
}

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    if (signal(SIGINT, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGINT handler.");
    }
    if (signal(SIGTERM, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGTERM handler.");
    }

    sd_bus *bus = NULL;
    if (sd_bus_default_system(&bus) < 0) {
        log_err("Failed to connect to system bus.");
        return 1;
    }

    loadHostInfo(bus);

    size_t interfaceCount = 0;
    linux_interface_ctx_t *interfaces = loadInterfaces(bus, &interfaceCount);
    if (!interfaces) {
        sd_bus_unref(bus);
        return 1;
    }

    sd_notify(0, "READY=1");

    while (!exitFlag) {
        sleep(1);
    }

    for (size_t i = 0; i < interfaceCount; i++) {
        pthread_join(interfaces[i].thread, NULL);
        if (interfaces[i].iface.socket >= 0) {
            close(interfaces[i].iface.socket);
        }
        free(interfaces[i].iface.recvBuffer);
        free(interfaces[i].iface.deviceName);
        free(interfaces[i].connectionUuid);
        freeAutomata(interfaces[i].mapping);
        freeAutomata(interfaces[i].session);
        freeAutomata(interfaces[i].enumeration);
    }
    free(interfaces);
    sd_bus_unref(bus);
    free(hostInfo.hostname);
    free(hostInfo.iconName);
    free(hostInfo.uuid);
    return 0;
}
