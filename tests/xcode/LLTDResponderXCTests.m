#import <XCTest/XCTest.h>

#include "lltdBlock.h"
#include "lltdEndian.h"
#include "lltdProtocol.h"
#include "lltdTestShim.h"
#include "lltdTlvOps.h"
#include "lltdWire.h"

static void fill_eth_addr(ethernet_address_t *addr, const uint8_t bytes[6]) {
    memcpy(addr->a, bytes, 6);
}

@interface LLTDResponderXCTests : XCTestCase
@end

@implementation LLTDResponderXCTests

- (void)setUp {
    [super setUp];
    lltd_test_port_reset();
}

- (void)testCompareEthernetAddress {
    ethernet_address_t a = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t b = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t c = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};

    XCTAssertTrue(compareEthernetAddress(&a, &b));
    XCTAssertFalse(compareEthernetAddress(&a, &c));
}

- (void)testSetLltdHeaderPopulatesFields {
    uint8_t buffer[sizeof(lltd_demultiplex_header_t)] = {0};
    ethernet_address_t source = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t destination = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};

    size_t written = setLltdHeader(buffer, &source, &destination, 0x1234, opcode_probe, tos_discovery);

    XCTAssertEqual(written, sizeof(lltd_demultiplex_header_t));
    lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)buffer;
    XCTAssertEqual(header->frameHeader.ethertype, lltd_htons(lltdEtherType));
    XCTAssertEqual(header->seqNumber, lltd_htons(0x1234));
    XCTAssertEqual(header->opcode, (uint8_t)opcode_probe);
    XCTAssertEqual(header->tos, (uint8_t)tos_discovery);
    XCTAssertEqual(header->version, (uint8_t)1);
    XCTAssertEqual(memcmp(&header->frameHeader.source, &source, sizeof(source)), 0);
    XCTAssertEqual(memcmp(&header->frameHeader.destination, &destination, sizeof(destination)), 0);
    XCTAssertEqual(memcmp(&header->realSource, &source, sizeof(source)), 0);
    XCTAssertEqual(memcmp(&header->realDestination, &destination, sizeof(destination)), 0);
}

- (void)testSetHelloHeaderWritesPayload {
    uint8_t buffer[sizeof(lltd_demultiplex_header_t) + sizeof(lltd_hello_upper_header_t)] = {0};
    ethernet_address_t apparent = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
    ethernet_address_t current = {{0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}};

    size_t written = setHelloHeader(buffer, sizeof(lltd_demultiplex_header_t), &apparent, &current, 0x42);

    XCTAssertEqual(written, sizeof(lltd_hello_upper_header_t));
    lltd_hello_upper_header_t *hello =
        (lltd_hello_upper_header_t *)(buffer + sizeof(lltd_demultiplex_header_t));
    XCTAssertEqual(memcmp(&hello->apparentMapper, &apparent, sizeof(apparent)), 0);
    XCTAssertEqual(memcmp(&hello->currentMapper, &current, sizeof(current)), 0);
    XCTAssertEqual(hello->generation, lltd_htons(0x42));
}

- (void)testEndOfPropertyTLVMarksBuffer {
    uint8_t buffer[4] = {0};
    size_t written = setEndOfPropertyTLV(buffer, 2);
    XCTAssertEqual(written, sizeof(uint8_t));
    XCTAssertEqual(buffer[2], (uint8_t)eofpropmarker);
}

- (void)testUuidTLVCorrectSize {
    uint8_t buffer[sizeof(generic_tlv_t) + 16 + 4] = {0};
    size_t written = setUuidTLV(buffer, 0);

    XCTAssertEqual(written, sizeof(generic_tlv_t) + 16);
    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    XCTAssertEqual(tlv->TLVType, (uint8_t)tlv_uuid);
    XCTAssertEqual(tlv->TLVLength, (uint8_t)16);
}

- (void)testPerfCounterTLVEndianness {
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint64_t)] = {0};
    size_t written = setPerfCounterTLV(buffer, 0);
    XCTAssertEqual(written, sizeof(generic_tlv_t) + sizeof(uint64_t));

    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    XCTAssertEqual(tlv->TLVType, (uint8_t)tlv_perfCounterFrequency);
    XCTAssertEqual(tlv->TLVLength, (uint8_t)sizeof(uint64_t));

    const uint8_t *bytes = buffer + sizeof(generic_tlv_t);
    uint64_t value =
        ((uint64_t)bytes[0] << 56) |
        ((uint64_t)bytes[1] << 48) |
        ((uint64_t)bytes[2] << 40) |
        ((uint64_t)bytes[3] << 32) |
        ((uint64_t)bytes[4] << 24) |
        ((uint64_t)bytes[5] << 16) |
        ((uint64_t)bytes[6] << 8) |
        (uint64_t)bytes[7];
    XCTAssertEqual(value, 1000000ULL);
}

- (void)testQosCharacteristicsTLVEndianness {
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint32_t)] = {0};
    size_t written = setQosCharacteristicsTLV(buffer, 0);
    XCTAssertEqual(written, sizeof(generic_tlv_t) + sizeof(uint32_t));

    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    XCTAssertEqual(tlv->TLVType, (uint8_t)tlv_qos_characteristics);
    XCTAssertEqual(tlv->TLVLength, (uint8_t)sizeof(uint32_t));
}

- (void)testIPv4TLVWritesIfaceAddress {
    lltd_test_iface_t iface;
    memset(&iface, 0, sizeof(iface));
    iface.ipv4_be = lltd_htonl(0x01020304);

    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint32_t)] = {0};
    size_t written = setIPv4TLV(buffer, 0, &iface);
    XCTAssertEqual(written, sizeof(generic_tlv_t) + sizeof(uint32_t));

    generic_tlv_t *hdr = (generic_tlv_t *)buffer;
    XCTAssertEqual(hdr->TLVType, (uint8_t)tlv_ipv4);
    XCTAssertEqual(hdr->TLVLength, (uint8_t)sizeof(uint32_t));

    const uint8_t *payload = buffer + sizeof(*hdr);
    XCTAssertEqual(payload[0], (uint8_t)0x01);
    XCTAssertEqual(payload[1], (uint8_t)0x02);
    XCTAssertEqual(payload[2], (uint8_t)0x03);
    XCTAssertEqual(payload[3], (uint8_t)0x04);
}

- (void)testIPv6TLVWritesIfaceAddress {
    lltd_test_iface_t iface;
    memset(&iface, 0, sizeof(iface));
    for (size_t i = 0; i < sizeof(iface.ipv6); i++) {
        iface.ipv6[i] = (uint8_t)(i + 1);
    }

    uint8_t buffer[sizeof(generic_tlv_t) + 16] = {0};
    size_t written = setIPv6TLV(buffer, 0, &iface);
    XCTAssertEqual(written, sizeof(generic_tlv_t) + 16);

    generic_tlv_t *hdr = (generic_tlv_t *)buffer;
    XCTAssertEqual(hdr->TLVType, (uint8_t)tlv_ipv6);
    XCTAssertEqual(hdr->TLVLength, (uint8_t)16);
    XCTAssertEqual(memcmp(buffer + sizeof(*hdr), iface.ipv6, 16), 0);
}

static void build_discover(uint8_t *buffer,
                           const uint8_t real_src[6],
                           uint8_t tos,
                           uint16_t xid,
                           uint16_t generation) {
    static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    lltd_demultiplex_header_t *hdr = (lltd_demultiplex_header_t *)buffer;
    lltd_discover_upper_header_t *disc = (lltd_discover_upper_header_t *)(buffer + sizeof(*hdr));
    memset(buffer, 0, sizeof(*hdr) + sizeof(*disc));

    hdr->frameHeader.ethertype = lltd_htons(lltdEtherType);
    fill_eth_addr(&hdr->frameHeader.source, real_src);
    fill_eth_addr(&hdr->frameHeader.destination, broadcast_mac);
    hdr->version = 1;
    hdr->tos = tos;
    hdr->opcode = opcode_discover;
    fill_eth_addr(&hdr->realSource, real_src);
    fill_eth_addr(&hdr->realDestination, broadcast_mac);
    hdr->seqNumber = lltd_htons(xid);

    disc->generation = lltd_htons(generation);
    disc->stationNumber = lltd_htons(0);
}

static uint16_t captured_hello_generation(void) {
    size_t len = 0;
    const void *frame = lltd_test_port_last_frame(&len);
    if (!frame) {
        return 0;
    }
    if (len < sizeof(lltd_demultiplex_header_t) + sizeof(lltd_hello_upper_header_t)) {
        return 0;
    }

    const lltd_demultiplex_header_t *hdr = (const lltd_demultiplex_header_t *)frame;
    if (hdr->opcode != opcode_hello) {
        return 0;
    }
    const lltd_hello_upper_header_t *hello =
        (const lltd_hello_upper_header_t *)((const uint8_t *)frame + sizeof(*hdr));
    return lltd_ntohs(hello->generation);
}

- (void)testDiscoverUpdatesGenerationInHello {
    lltd_test_iface_t iface;
    memset(&iface, 0, sizeof(iface));
    memcpy(iface.mac, (const uint8_t[]){0xde, 0xad, 0xbe, 0xef, 0x00, 0x01}, 6);
    iface.mtu = 1500;

    uint8_t frame[sizeof(lltd_demultiplex_header_t) + sizeof(lltd_discover_upper_header_t)];
    const uint8_t mapper_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    build_discover(frame, mapper_mac, tos_discovery, 1, 0x0000);
    parseFrame(frame, &iface);
    XCTAssertEqual(lltd_test_port_send_count(), 1);
    XCTAssertEqual(captured_hello_generation(), (uint16_t)0);

    lltd_test_port_reset();
    build_discover(frame, mapper_mac, tos_discovery, 2, 0xfdc1);
    parseFrame(frame, &iface);
    XCTAssertEqual(lltd_test_port_send_count(), 1);
    XCTAssertEqual(captured_hello_generation(), (uint16_t)0xfdc1);
}

@end

