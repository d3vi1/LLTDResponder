/******************************************************************************
 *                                                                            *
 *   freebsd-main.c                                                           *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"
#include "../lltdAutomata.h"
#include "../lltdBlock.h"

#include <errno.h>
#include <ifaddrs.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t exitFlag = 0;

global_info_t globalInfo = {0};

typedef struct {
    network_interface_t iface;
    pthread_t thread;
    automata *mapping;
    automata *session;
    automata *enumeration;
} freebsd_interface_ctx_t;

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

static int openBpfDevice(void) {
    char path[16];
    for (int i = 0; i < 255; i++) {
        snprintf(path, sizeof(path), "/dev/bpf%d", i);
        int fd = open(path, O_RDWR);
        if (fd >= 0) {
            return fd;
        }
    }
    return -1;
}

static bool fillInterfaceDetails(network_interface_t *iface, const char *ifname) {
    struct ifreq ifr;

    iface->deviceName = strdup(ifname);
    if (!iface->deviceName) {
        return false;
    }

    int bpf = openBpfDevice();
    if (bpf < 0) {
        log_err("Failed to open /dev/bpf for %s", ifname);
        return false;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    if (ioctl(bpf, BIOCSETIF, &ifr) < 0) {
        log_err("BIOCSETIF failed on %s: %s", ifname, strerror(errno));
        close(bpf);
        return false;
    }

    u_int enable = 1;
    if (ioctl(bpf, BIOCIMMEDIATE, &enable) < 0) {
        log_err("BIOCIMMEDIATE failed on %s: %s", ifname, strerror(errno));
    }
    if (ioctl(bpf, BIOCSHDRCMPLT, &enable) < 0) {
        log_err("BIOCSHDRCMPLT failed on %s: %s", ifname, strerror(errno));
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
        if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
            iface->MTU = ifr.ifr_mtu;
        } else {
            iface->MTU = 1500;
        }
        if (ioctl(sock, SIOCGIFADDR, &ifr) == 0 && ifr.ifr_addr.sa_family == AF_LINK) {
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)&ifr.ifr_addr;
            memcpy(iface->macAddress, LLADDR(sdl), sizeof(iface->macAddress));
        }
        close(sock);
    } else {
        iface->MTU = 1500;
    }

    iface->socket = bpf;
    memset(&iface->socketAddr, 0, sizeof(iface->socketAddr));
    iface->recvBufferSize = iface->MTU + sizeof(struct bpf_hdr);
    iface->recvBuffer = malloc(iface->recvBufferSize);
    if (!iface->recvBuffer) {
        close(bpf);
        return false;
    }

    iface->seeList = NULL;
    iface->seeListCount = 0;
    iface->MapperSeqNumber = 0;
    memset(iface->MapperHwAddress, 0, sizeof(iface->MapperHwAddress));

    log_notice("FreeBSD LLTD socket on %s", ifname);
    return true;
}

static void *lltdLoop(void *data) {
    freebsd_interface_ctx_t *ctx = data;
    network_interface_t *iface = &ctx->iface;

    while (!exitFlag) {
        ssize_t bytes = read(iface->socket, iface->recvBuffer, iface->recvBufferSize);
        if (bytes <= 0) {
            continue;
        }

        uint8_t *cursor = iface->recvBuffer;
        while (cursor < (uint8_t *)iface->recvBuffer + bytes) {
            struct bpf_hdr *hdr = (struct bpf_hdr *)cursor;
            uint8_t *frame = cursor + hdr->bh_hdrlen;
            if (hdr->bh_caplen >= sizeof(lltd_demultiplex_header_t)) {
                lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)frame;
                switch_state_mapping(ctx->mapping, header->opcode, "rx");
                switch_state_session(ctx->session, header->opcode, "rx");
                if (header->opcode == opcode_hello) {
                    switch_state_enumeration(ctx->enumeration, enum_hello, "rx");
                } else if (header->opcode == opcode_discover) {
                    switch_state_enumeration(ctx->enumeration, enum_new_session, "rx");
                } else {
                    switch_state_enumeration(ctx->enumeration, enum_sess_complete, "rx");
                }
                parseFrame(frame, iface);
            }
            cursor += BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen);
        }
    }

    return NULL;
}

static char **listInterfaces(size_t *count) {
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
    (void)argc;
    (void)argv;

    if (signal(SIGINT, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGINT handler.");
    }
    if (signal(SIGTERM, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGTERM handler.");
    }

    size_t interfaceCount = 0;
    char **names = listInterfaces(&interfaceCount);
    if (!names || interfaceCount == 0) {
        log_err("No active interfaces found.");
        return 1;
    }

    freebsd_interface_ctx_t *interfaces = calloc(interfaceCount, sizeof(*interfaces));
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
        interfaces[startedCount].enumeration = init_automata_enumeration();
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
        freeAutomata(interfaces[i].enumeration);
    }
    free(interfaces);
    return 0;
}
