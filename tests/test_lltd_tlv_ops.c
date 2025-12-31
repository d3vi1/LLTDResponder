#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lltdTestShim.h"
#include "lltdTlvOps.h"
#include "test_utils.h"

static bool test_compare_ethernet_address_equal(void) {
    ethernet_address_t a = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t b = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT(compareEthernetAddress(&a, &b));
    return true;
}

static bool test_compare_ethernet_address_not_equal(void) {
    ethernet_address_t a = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t b = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};
    TEST_ASSERT(!compareEthernetAddress(&a, &b));
    return true;
}

static bool test_set_lltd_header_matches_fixture(void) {
    uint8_t buffer[sizeof(lltd_demultiplex_header_t)] = {0};
    ethernet_address_t source = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t destination = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};

    size_t written = setLltdHeader(buffer, &source, &destination, 0x1234, opcode_probe, tos_discovery);
    TEST_ASSERT_EQ_SIZE(written, sizeof(lltd_demultiplex_header_t));

    const uint8_t expected[sizeof(lltd_demultiplex_header_t)] = {
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x88, 0xd9, 0x01, 0x00, 0x00, 0x04,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x34, 0x12
    };
    TEST_ASSERT(memcmp(buffer, expected, sizeof(expected)) == 0);
    return true;
}

static bool test_set_hello_header_matches_fixture(void) {
    uint8_t buffer[sizeof(lltd_hello_upper_header_t)] = {0};
    ethernet_address_t apparent = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
    ethernet_address_t current = {{0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}};

    size_t written = setHelloHeader(buffer, 0, &apparent, &current, 0x42);
    TEST_ASSERT_EQ_SIZE(written, sizeof(lltd_hello_upper_header_t));

    const uint8_t expected[sizeof(lltd_hello_upper_header_t)] = {
        0x42, 0x00,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06
    };
    TEST_ASSERT(memcmp(buffer, expected, sizeof(expected)) == 0);
    return true;
}

static bool test_set_end_of_property_tlv_marks_buffer(void) {
    uint8_t buffer[4] = {0};
    size_t written = setEndOfPropertyTLV(buffer, 2);
    TEST_ASSERT_EQ_SIZE(written, sizeof(uint8_t));
    TEST_ASSERT_EQ_INT(buffer[2], eofpropmarker);
    return true;
}

int main(void) {
    const struct test_case cases[] = {
        {"compare_equal", test_compare_ethernet_address_equal},
        {"compare_not_equal", test_compare_ethernet_address_not_equal},
        {"lltd_header_fixture", test_set_lltd_header_matches_fixture},
        {"hello_header_fixture", test_set_hello_header_matches_fixture},
        {"end_of_property", test_set_end_of_property_tlv_marks_buffer},
    };

    return run_tests(cases, sizeof(cases) / sizeof(cases[0]));
}
