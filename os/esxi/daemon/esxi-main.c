/******************************************************************************
 *                                                                            *
 *   esxi-main.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "../../../lltdResponder/lltdAutomata.h"
#include "../../daemon/lltdBlock.h"

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
    network_interface_t currentNetworkInterface;
    pthread_t           posixThreadID;
    automata           *mappingAutomata;
    automata           *sessionAutomata;
    automata           *enumerationAutomata;
} esxi_interface_t;

static void signalHandler(int Signal) {
    log_debug("Interrupted by signal #%d", Signal);
    exitFlag = 1;
}

static void freeAutomata(automata *autom) {
    if (!autom) {
        return;
    }
    free(autom->extra);
    free(autom);
}

static boolean_t fillInterfaceDetails(network_interface_t *currentNetworkInterface, const char *interfaceName) {
    struct ifreq IfRequest;
    memset(&IfRequest, 0, sizeof(IfRequest));

    currentNetworkInterface->deviceName = strdup(interfaceName);
    currentNetworkInterface->ifIndex = if_nametoindex(interfaceName);
    if (!currentNetworkInterface->deviceName || currentNetworkInterface->ifIndex == 0) {
        return false;
    }

    currentNetworkInterface->socket = socket(AF_PACKET, SOCK_RAW, htons(lltdEtherType));
    if (currentNetworkInterface->socket < 0) {
        log_err("Could not create packet socket on %s: %s", interfaceName, strerror(errno));
        return false;
    }

    memset(&currentNetworkInterface->socketAddr, 0, sizeof(currentNetworkInterface->socketAddr));
    currentNetworkInterface->socketAddr.sll_family = AF_PACKET;
    currentNetworkInterface->socketAddr.sll_protocol = htons(lltdEtherType);
    currentNetworkInterface->socketAddr.sll_ifindex = currentNetworkInterface->ifIndex;
    currentNetworkInterface->socketAddr.sll_halen = ETH_ALEN;
    memset(currentNetworkInterface->socketAddr.sll_addr, 0xFF, ETH_ALEN);

    if (bind(currentNetworkInterface->socket,
             (struct sockaddr *)&currentNetworkInterface->socketAddr,
             sizeof(currentNetworkInterface->socketAddr)) < 0) {
        log_err("Could not bind packet socket on %s: %s", interfaceName, strerror(errno));
        close(currentNetworkInterface->socket);
        return false;
    }

    strncpy(IfRequest.ifr_name, interfaceName, sizeof(IfRequest.ifr_name) - 1);
    if (ioctl(currentNetworkInterface->socket, SIOCGIFMTU, &IfRequest) == 0) {
        currentNetworkInterface->MTU = IfRequest.ifr_mtu;
    } else {
        currentNetworkInterface->MTU = 1500;
    }

    if (ioctl(currentNetworkInterface->socket, SIOCGIFHWADDR, &IfRequest) == 0) {
        memcpy(currentNetworkInterface->macAddress, IfRequest.ifr_hwaddr.sa_data, ETH_ALEN);
    }

    currentNetworkInterface->recvBuffer = malloc(currentNetworkInterface->MTU);
    if (!currentNetworkInterface->recvBuffer) {
        close(currentNetworkInterface->socket);
        return false;
    }

    currentNetworkInterface->seeList = NULL;
    currentNetworkInterface->seeListCount = 0;
    currentNetworkInterface->MapperSeqNumber = 0;
    memset(currentNetworkInterface->MapperHwAddress, 0, sizeof(currentNetworkInterface->MapperHwAddress));

    log_notice("ESXi LLTD socket on %s", interfaceName);
    return true;
}

static void *lltdLoop(void *data) {
    esxi_interface_t *interfaceState = data;
    network_interface_t *currentNetworkInterface = &interfaceState->currentNetworkInterface;

    while (!exitFlag) {
        ssize_t bytes = recvfrom(currentNetworkInterface->socket,
                                 currentNetworkInterface->recvBuffer,
                                 currentNetworkInterface->MTU,
                                 0,
                                 NULL,
                                 NULL);
        if (bytes <= 0) {
            continue;
        }
        lltd_demultiplex_header_t *header = currentNetworkInterface->recvBuffer;
        switch_state_mapping(interfaceState->mappingAutomata, header->opcode, "rx");
        switch_state_session(interfaceState->sessionAutomata, header->opcode, "rx");
        if (header->opcode == opcode_hello) {
            switch_state_enumeration(interfaceState->enumerationAutomata, enum_hello, "rx");
        } else if (header->opcode == opcode_discover) {
            switch_state_enumeration(interfaceState->enumerationAutomata, enum_new_session, "rx");
        } else {
            switch_state_enumeration(interfaceState->enumerationAutomata, enum_sess_complete, "rx");
        }
        parseFrame(currentNetworkInterface->recvBuffer, currentNetworkInterface);
    }

    return NULL;
}

static char **listInterfaces(size_t *count) {
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) {
        return NULL;
    }

    char **names = NULL;
    size_t nameCount = 0;

    for (struct ifaddrs *current = interfaces; current != NULL; current = current->ifa_next) {
        if (!current->ifa_name || !(current->ifa_flags & IFF_UP) || (current->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }

        boolean_t exists = false;
        for (size_t i = 0; i < nameCount; i++) {
            if (strcmp(names[i], current->ifa_name) == 0) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        char **expanded = realloc(names, sizeof(*names) * (nameCount + 1));
        if (!expanded) {
            continue;
        }
        names = expanded;
        names[nameCount] = strdup(current->ifa_name);
        if (names[nameCount]) {
            nameCount++;
        }
    }

    freeifaddrs(interfaces);
    *count = nameCount;
    return names;
}

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    if (signal(SIGINT, signalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGINT handler.");
    }
    if (signal(SIGTERM, signalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGTERM handler.");
    }

    size_t interfaceCount = 0;
    char **interfaces = listInterfaces(&interfaceCount);
    if (!interfaces || interfaceCount == 0) {
        log_err("No active interfaces found.");
        return 1;
    }

    esxi_interface_t *states = calloc(interfaceCount, sizeof(*states));
    if (!states) {
        return 1;
    }

    size_t started = 0;
    for (size_t i = 0; i < interfaceCount; i++) {
        if (!fillInterfaceDetails(&states[started].currentNetworkInterface, interfaces[i])) {
            free(interfaces[i]);
            continue;
        }
        states[started].mappingAutomata = init_automata_mapping();
        states[started].sessionAutomata = init_automata_session();
        states[started].enumerationAutomata = init_automata_enumeration();
        pthread_create(&states[started].posixThreadID, NULL, lltdLoop, &states[started]);
        started++;
        free(interfaces[i]);
    }
    free(interfaces);

    while (!exitFlag) {
        sleep(1);
    }

    for (size_t i = 0; i < started; i++) {
        pthread_join(states[i].posixThreadID, NULL);
        close(states[i].currentNetworkInterface.socket);
        free(states[i].currentNetworkInterface.recvBuffer);
        free((void *)states[i].currentNetworkInterface.deviceName);
        freeAutomata(states[i].mappingAutomata);
        freeAutomata(states[i].sessionAutomata);
        freeAutomata(states[i].enumerationAutomata);
    }
    free(states);
    return 0;
}
