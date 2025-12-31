/******************************************************************************
 *                                                                            *
 *   beos-shared.c                                                            *
 *   lltdDaemon                                                               *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"
#include "../lltdAutomata.h"
#include "../lltdBlock.h"
#include "beos-shared.h"

#include <errno.h>
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
    automata *mappingAutomata;
    automata *sessionAutomata;
    automata *enumerationAutomata;
} beos_interface_state_t;

static void freeAutomata(automata *autom) {
    if (!autom) {
        return;
    }
    free(autom->extra);
    free(autom);
}

static void SignalHandler(int Signal) {
    log_debug("Interrupted by signal #%d", Signal);
    exitFlag = 1;
}

static bool fillInterfaceDetails(network_interface_t *iface, const char *ifname) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    iface->deviceName = strdup(ifname);
    iface->ifIndex = if_nametoindex(ifname);
    if (!iface->deviceName || iface->ifIndex == 0) {
        return false;
    }

    iface->socket = socket(AF_PACKET, SOCK_RAW, htons(lltdEtherType));
    if (iface->socket < 0) {
        log_err("Could not create packet socket on %s: %s", ifname, strerror(errno));
        return false;
    }

    memset(&iface->socketAddr, 0, sizeof(iface->socketAddr));
    iface->socketAddr.sll_family = AF_PACKET;
    iface->socketAddr.sll_protocol = htons(lltdEtherType);
    iface->socketAddr.sll_ifindex = iface->ifIndex;
    iface->socketAddr.sll_halen = 6;
    memset(iface->socketAddr.sll_addr, 0xFF, 6);

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
        memcpy(iface->macAddress, ifr.ifr_hwaddr.sa_data, 6);
    }

    iface->recvBuffer = malloc(iface->MTU);
    if (!iface->recvBuffer) {
        close(iface->socket);
        return false;
    }

    iface->seeList = NULL;
    iface->seeListCount = 0;
    iface->MapperSeqNumber = 0;
    memset(iface->MapperHwAddress, 0, sizeof(iface->MapperHwAddress));

    log_notice("BeOS/Haiku LLTD socket on %s", ifname);
    return true;
}

static void *lltdLoop(void *data) {
    beos_interface_state_t *state = data;
    network_interface_t *iface = &state->iface;

    while (!exitFlag) {
        ssize_t bytes = recvfrom(iface->socket, iface->recvBuffer, iface->MTU, 0, NULL, NULL);
        if (bytes <= 0) {
            continue;
        }
        lltd_demultiplex_header_t *header = iface->recvBuffer;
        switch_state_mapping(state->mappingAutomata, header->opcode, "rx");
        switch_state_session(state->sessionAutomata, header->opcode, "rx");
        if (header->opcode == opcode_hello) {
            switch_state_enumeration(state->enumerationAutomata, enum_hello, "rx");
        } else if (header->opcode == opcode_discover) {
            switch_state_enumeration(state->enumerationAutomata, enum_new_session, "rx");
        } else {
            switch_state_enumeration(state->enumerationAutomata, enum_sess_complete, "rx");
        }
        parseFrame(iface->recvBuffer, iface);
    }

    return NULL;
}

int runBeosResponder(beos_interface_list_t *(*listInterfaces)(void), void (*freeList)(beos_interface_list_t *)) {
    if (signal(SIGINT, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGINT handler.");
    }
    if (signal(SIGTERM, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGTERM handler.");
    }

    beos_interface_list_t *list = listInterfaces();
    if (!list || list->count == 0) {
        log_err("No active interfaces found.");
        if (list && freeList) {
            freeList(list);
        }
        return 1;
    }

    beos_interface_state_t *states = calloc(list->count, sizeof(*states));
    if (!states) {
        freeList(list);
        return 1;
    }

    size_t startedCount = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (!fillInterfaceDetails(&states[startedCount].iface, list->names[i])) {
            continue;
        }
        states[startedCount].mappingAutomata = init_automata_mapping();
        states[startedCount].sessionAutomata = init_automata_session();
        states[startedCount].enumerationAutomata = init_automata_enumeration();
        pthread_create(&states[startedCount].thread, NULL, lltdLoop, &states[startedCount]);
        startedCount++;
    }

    while (!exitFlag) {
        sleep(1);
    }

    for (size_t i = 0; i < startedCount; i++) {
        pthread_join(states[i].thread, NULL);
        close(states[i].iface.socket);
        free(states[i].iface.recvBuffer);
        free(states[i].iface.deviceName);
        freeAutomata(states[i].mappingAutomata);
        freeAutomata(states[i].sessionAutomata);
        freeAutomata(states[i].enumerationAutomata);
    }
    free(states);
    freeList(list);
    return 0;
}
