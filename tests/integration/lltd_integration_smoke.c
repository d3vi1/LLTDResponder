#include <errno.h>
#ifdef __linux__
#include <linux/if_ether.h>
#endif
#include <net/if.h>
#include <netpacket/packet.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lltdTestShim.h"
#include "lltdEndian.h"
#include "lltdWire.h"

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

static int open_bound_socket(const char *ifname, struct sockaddr_ll *addr) {
    int fd = socket(AF_PACKET, SOCK_RAW, lltd_htons(lltdEtherType));
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sll_family = AF_PACKET;
    addr->sll_protocol = lltd_htons(lltdEtherType);
    addr->sll_ifindex = if_nametoindex(ifname);
    addr->sll_halen = ETH_ALEN;
    memset(addr->sll_addr, 0xff, ETH_ALEN);

    if (addr->sll_ifindex == 0) {
        fprintf(stderr, "Unknown interface %s\n", ifname);
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    return fd;
}

static bool get_interface_mac(const char *ifname, ethernet_address_t *mac) {
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket(AF_INET)");
        return false;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl(SIOCGIFHWADDR)");
        close(fd);
        return false;
    }
    memcpy(mac->a, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    close(fd);
    return true;
}

int main(int argc, char **argv) {
    const char *ifname = NULL;
    int timeout_ms = 2000;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0) && i + 1 < argc) {
            ifname = argv[i + 1];
            i++;
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) && i + 1 < argc) {
            timeout_ms = atoi(argv[i + 1]);
            i++;
        }
    }

    if (!ifname) {
        fprintf(stderr, "Usage: %s --interface <ifname> [--timeout ms]\n", argv[0]);
        return 1;
    }

    struct sockaddr_ll addr;
    int fd = open_bound_socket(ifname, &addr);
    if (fd < 0) {
        return 1;
    }

    ethernet_address_t source = {{0}};
    if (!get_interface_mac(ifname, &source)) {
        close(fd);
        return 1;
    }

    ethernet_address_t destination = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
    uint8_t buffer[sizeof(lltd_demultiplex_header_t) + sizeof(lltd_discover_upper_header_t)] = {0};

    size_t offset = setLltdHeader(buffer, &source, &destination, 0x0001, opcode_discover, tos_discovery);
    lltd_discover_upper_header_t *discover = (lltd_discover_upper_header_t *)(buffer + offset);
    discover->generation = 1;
    discover->stationNumber = 0;
    offset += sizeof(*discover);

    ssize_t sent = sendto(fd, buffer, offset, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        perror("sendto");
        close(fd);
        return 1;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    int poll_rc = poll(&pfd, 1, timeout_ms);
    if (poll_rc <= 0) {
        fprintf(stderr, "Timed out waiting for LLTD hello\n");
        close(fd);
        return 1;
    }

    uint8_t recvbuf[2048];
    ssize_t recvlen = recvfrom(fd, recvbuf, sizeof(recvbuf), 0, NULL, NULL);
    if (recvlen < (ssize_t)sizeof(lltd_demultiplex_header_t)) {
        fprintf(stderr, "Short read from socket\n");
        close(fd);
        return 1;
    }

    lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)recvbuf;
    if (lltd_ntohs(header->frameHeader.ethertype) != lltdEtherType ||
        header->opcode != opcode_hello ||
        header->tos != tos_discovery) {
        fprintf(stderr, "Unexpected response opcode=%u tos=%u ethertype=0x%04x\n",
                header->opcode, header->tos, lltd_ntohs(header->frameHeader.ethertype));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
