#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "lltdTestShim.h"
#include "lltdTlvOps.h"

static void test_compare_ethernet_address_equal(void **state) {
    (void)state;
    ethernet_address_t a = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t b = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};

    assert_true(compareEthernetAddress(&a, &b));
}

static void test_compare_ethernet_address_not_equal(void **state) {
    (void)state;
    ethernet_address_t a = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t b = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};

    assert_false(compareEthernetAddress(&a, &b));
}

static void test_set_lltd_header_populates_fields(void **state) {
    (void)state;
    uint8_t buffer[sizeof(lltd_demultiplex_header_t)] = {0};
    ethernet_address_t source = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    ethernet_address_t destination = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};

    size_t written = setLltdHeader(buffer, &source, &destination, 0x1234, opcode_probe, tos_discovery);

    assert_int_equal(written, sizeof(lltd_demultiplex_header_t));
    lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)buffer;
    assert_int_equal(header->frameHeader.ethertype, htons(lltdEtherType));
    assert_memory_equal(&header->frameHeader.source, &source, sizeof(source));
    assert_memory_equal(&header->frameHeader.destination, &destination, sizeof(destination));
    assert_memory_equal(&header->realSource, &source, sizeof(source));
    assert_memory_equal(&header->realDestination, &destination, sizeof(destination));
    assert_int_equal(header->seqNumber, 0x1234);
    assert_int_equal(header->opcode, opcode_probe);
    assert_int_equal(header->tos, tos_discovery);
    assert_int_equal(header->version, 1);
}

static void test_set_hello_header_writes_payload(void **state) {
    (void)state;
    uint8_t buffer[sizeof(lltd_demultiplex_header_t) + sizeof(lltd_hello_upper_header_t)] = {0};
    ethernet_address_t apparent = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
    ethernet_address_t current = {{0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}};

    size_t written = setHelloHeader(buffer, sizeof(lltd_demultiplex_header_t), &apparent, &current, 0x42);

    assert_int_equal(written, sizeof(lltd_hello_upper_header_t));
    lltd_hello_upper_header_t *hello =
        (lltd_hello_upper_header_t *)(buffer + sizeof(lltd_demultiplex_header_t));
    assert_memory_equal(&hello->apparentMapper, &apparent, sizeof(apparent));
    assert_memory_equal(&hello->currentMapper, &current, sizeof(current));
    assert_int_equal(hello->generation, 0x42);
}

static void test_set_end_of_property_tlv_marks_buffer(void **state) {
    (void)state;
    uint8_t buffer[4] = {0};

    size_t written = setEndOfPropertyTLV(buffer, 2);

    assert_int_equal(written, sizeof(uint8_t));
    assert_int_equal(buffer[2], eofpropmarker);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_compare_ethernet_address_equal),
        cmocka_unit_test(test_compare_ethernet_address_not_equal),
        cmocka_unit_test(test_set_lltd_header_populates_fields),
        cmocka_unit_test(test_set_hello_header_writes_payload),
        cmocka_unit_test(test_set_end_of_property_tlv_marks_buffer),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
