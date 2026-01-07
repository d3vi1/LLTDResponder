#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#include "lltdTestShim.h"
#include "lltdBlock.h"

static void fill_eth_addr(ethernet_address_t *addr, const uint8_t *bytes) {
    memcpy(addr->a, bytes, sizeof(addr->a));
}

static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static void build_discover(uint8_t *buffer,
                           const uint8_t *real_src,
                           uint8_t tos,
                           uint16_t xid,
                           uint16_t generation) {
    lltd_demultiplex_header_t *hdr = (lltd_demultiplex_header_t *)buffer;
    lltd_discover_upper_header_t *disc = (lltd_discover_upper_header_t *)(buffer + sizeof(*hdr));
    memset(buffer, 0, sizeof(*hdr) + sizeof(*disc));

    hdr->frameHeader.ethertype = htons(lltdEtherType);
    fill_eth_addr(&hdr->frameHeader.source, real_src);
    fill_eth_addr(&hdr->frameHeader.destination, broadcast_mac);
    hdr->version = 1;
    hdr->tos = tos;
    hdr->opcode = opcode_discover;
    fill_eth_addr(&hdr->realSource, real_src);
    fill_eth_addr(&hdr->realDestination, broadcast_mac);
    hdr->seqNumber = htons(xid);

    disc->generation = htons(generation);
    disc->stationNumber = htons(0);
}

static void build_hello(uint8_t *buffer,
                        const uint8_t *real_src,
                        uint8_t tos,
                        uint16_t generation) {
    lltd_demultiplex_header_t *hdr = (lltd_demultiplex_header_t *)buffer;
    lltd_hello_upper_header_t *hello = (lltd_hello_upper_header_t *)(buffer + sizeof(*hdr));
    memset(buffer, 0, sizeof(*hdr) + sizeof(*hello));

    hdr->frameHeader.ethertype = htons(lltdEtherType);
    fill_eth_addr(&hdr->frameHeader.source, real_src);
    fill_eth_addr(&hdr->frameHeader.destination, broadcast_mac);
    hdr->version = 1;
    hdr->tos = tos;
    hdr->opcode = opcode_hello;
    fill_eth_addr(&hdr->realSource, real_src);
    fill_eth_addr(&hdr->realDestination, broadcast_mac);
    hdr->seqNumber = htons(0);

    hello->generation = htons(generation);
}

int main(void) {
    network_interface_t iface;
    uint8_t buffer[sizeof(lltd_demultiplex_header_t) + sizeof(lltd_discover_upper_header_t)];
    uint8_t hello_buffer[sizeof(lltd_demultiplex_header_t) + sizeof(lltd_hello_upper_header_t)];
    const uint8_t mapper_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const uint8_t other_mac[6] = {0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};

    memset(&iface, 0, sizeof(iface));
    iface.deviceName = "test0";
    iface.helloSent = 1; /* suppress answerHello() in parseFrame */

    build_discover(buffer, mapper_mac, tos_discovery, 1, 0x0000);
    parseFrame(buffer, &iface);
    assert(iface.MapperGenerationTopology == 0);

    build_hello(hello_buffer, other_mac, tos_discovery, 0xe8c5);
    parseFrame(hello_buffer, &iface);
    assert(iface.MapperGenerationTopology == 0);

    build_discover(buffer, mapper_mac, tos_discovery, 2, 0xfdc1);
    parseFrame(buffer, &iface);
    assert(iface.MapperGenerationTopology == 0xfdc1);

    return 0;
}
