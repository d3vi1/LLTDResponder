/******************************************************************************
 *                                                                            *
 *   linux-embedded-main.c                                                    *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Minimal Linux embedded implementation using classic UAPI.               *
 *                                                                            *
 ******************************************************************************/

#define LLTD_USE_CONSOLE 1

#include "../lltdDaemon.h"
#include "../lltdAutomata.h"
#include "../lltdBlock.h"

#include <errno.h>
#include <ifaddrs.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t exitFlag = 0;

typedef struct {
    network_interface_t iface;
    pthread_t thread;
    automata *mapping;
    automata *session;
} embedded_interface_ctx_t;

static void SignalHandler(int signal) {
    log_debug("Interrupted by signal #%d", signal);
    exitFlag = 1;
}

static void freeAutomata(automata *autom) {
    if (!autom) {
        return;
    }
    free(autom->extra);
    free(autom);
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

    log_notice("Embedded LLTD socket on %s (ifindex=%d)", ifname, iface->ifIndex);
    return true;
}

static void *lltdLoop(void *data) {
    embedded_interface_ctx_t *ctx = data;
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

static char **listInterfaces(size_t *count, const char *only_name) {
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) != 0) {
        return NULL;
    }

    char **names = NULL;
    size_t interfaceCount = 0;

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_name || !(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }
        if (only_name && strcmp(only_name, ifa->ifa_name) != 0) {
            continue;
        }

        bool exists = false;
        for (size_t i = 0; i < interfaceCount; i++) {
            if (strcmp(names[i], ifa->ifa_name) == 0) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        char **expanded = realloc(names, sizeof(*names) * (interfaceCount + 1));
        if (!expanded) {
            continue;
        }
        names = expanded;
        names[interfaceCount] = strdup(ifa->ifa_name);
        if (names[interfaceCount]) {
            interfaceCount++;
        }
    }

    freeifaddrs(ifaddr);
    *count = interfaceCount;
    return names;
}

int main(int argc, const char *argv[]) {
    const char *only_interface = getenv("LLTD_INTERFACE");
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0) && i + 1 < argc) {
            only_interface = argv[i + 1];
            i++;
        }
    }

    if (signal(SIGINT, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGINT handler.");
    }
    if (signal(SIGTERM, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGTERM handler.");
    }

    size_t interfaceCount = 0;
    char **names = listInterfaces(&interfaceCount, only_interface);
    if (!names || interfaceCount == 0) {
        log_err("No active interfaces found.");
        return 1;
    }

    embedded_interface_ctx_t *interfaces = calloc(interfaceCount, sizeof(*interfaces));
    if (!interfaces) {
        return 1;
    }

    size_t startedCount = 0;
    for (size_t i = 0; i < interfaceCount; i++) {
        if (!fillInterfaceDetails(&interfaces[startedCount].iface, names[i])) {
            free(names[i]);
            continue;
        }
        interfaces[startedCount].mapping = init_automata_mapping();
        interfaces[startedCount].session = init_automata_session();
        pthread_create(&interfaces[startedCount].thread, NULL, lltdLoop, &interfaces[startedCount]);
        startedCount++;
        free(names[i]);
    }
    free(names);

    while (!exitFlag) {
        sleep(1);
    }

    for (size_t i = 0; i < startedCount; i++) {
        pthread_join(interfaces[i].thread, NULL);
        close(interfaces[i].iface.socket);
        free(interfaces[i].iface.recvBuffer);
        free(interfaces[i].iface.deviceName);
        freeAutomata(interfaces[i].mapping);
        freeAutomata(interfaces[i].session);
    }
    free(interfaces);
    return 0;
}
