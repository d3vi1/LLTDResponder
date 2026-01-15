#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include "lltdTestShim.h"
#include "lltdEndian.h"
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
    assert_int_equal(header->frameHeader.ethertype, lltd_htons(lltdEtherType));
    assert_memory_equal(&header->frameHeader.source, &source, sizeof(source));
    assert_memory_equal(&header->frameHeader.destination, &destination, sizeof(destination));
    assert_memory_equal(&header->realSource, &source, sizeof(source));
    assert_memory_equal(&header->realDestination, &destination, sizeof(destination));
    assert_int_equal(header->seqNumber, lltd_htons(0x1234));
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
    assert_int_equal(hello->generation, lltd_htons(0x42));
}

static void test_set_end_of_property_tlv_marks_buffer(void **state) {
    (void)state;
    uint8_t buffer[4] = {0};

    size_t written = setEndOfPropertyTLV(buffer, 2);

    assert_int_equal(written, sizeof(uint8_t));
    assert_int_equal(buffer[2], eofpropmarker);
}

// Test UUID TLV is exactly 16 bytes payload when UUID available
static void test_set_uuid_tlv_correct_size(void **state) {
    (void)state;
    uint8_t buffer[sizeof(generic_tlv_t) + 16 + 4] = {0};

    size_t written = setUuidTLV(buffer, 0);

    // Either 16 bytes payload + header, or just header if no UUID
    if (written > sizeof(generic_tlv_t)) {
        assert_int_equal(written, sizeof(generic_tlv_t) + 16);
        generic_tlv_t *tlv = (generic_tlv_t *)buffer;
        assert_int_equal(tlv->TLVType, tlv_uuid);
        assert_int_equal(tlv->TLVLength, 16);
    } else {
        // No UUID available (on non-Apple or testing platform)
        assert_true(written == 0 || written == sizeof(generic_tlv_t));
    }
}

// Test that perf counter TLV uses network byte order (big-endian)
static void test_set_perf_counter_tlv_endianness(void **state) {
    (void)state;
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint64_t)] = {0};

    size_t written = setPerfCounterTLV(buffer, 0);

    assert_int_equal(written, sizeof(generic_tlv_t) + sizeof(uint64_t));
    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    assert_int_equal(tlv->TLVType, tlv_perfCounterFrequency);
    assert_int_equal(tlv->TLVLength, sizeof(uint64_t));

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
    assert_int_equal(value, 1000000ULL);
}

// Test QoS characteristics TLV uses network byte order
static void test_set_qos_characteristics_tlv_endianness(void **state) {
    (void)state;
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint32_t)] = {0};

    size_t written = setQosCharacteristicsTLV(buffer, 0);

    assert_int_equal(written, sizeof(generic_tlv_t) + sizeof(uint32_t));
    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    assert_int_equal(tlv->TLVType, tlv_qos_characteristics);
    assert_int_equal(tlv->TLVLength, sizeof(uint32_t));

    uint32_t *value = (uint32_t *)(buffer + sizeof(generic_tlv_t));
    uint32_t expected = (uint32_t)(Config_TLV_QOS_L2Fwd | Config_TLV_QOS_PrioTag | Config_TLV_QOS_VLAN) << 16;
    assert_int_equal(*value, htonl(expected));
}

// Test icon image TLV has zero length (empty stub)
static void test_set_icon_image_tlv_empty(void **state) {
    (void)state;
    uint8_t buffer[sizeof(generic_tlv_t)] = {0};

    size_t written = setIconImageTLV(buffer, 0);

    assert_int_equal(written, sizeof(generic_tlv_t));
    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    assert_int_equal(tlv->TLVType, tlv_iconImage);
    assert_int_equal(tlv->TLVLength, 0);
}

// Test friendly name TLV has zero length (empty stub)
static void test_set_friendly_name_tlv_empty(void **state) {
    (void)state;
    uint8_t buffer[sizeof(generic_tlv_t)] = {0};

    size_t written = setFriendlyNameTLV(buffer, 0);

    assert_int_equal(written, sizeof(generic_tlv_t));
    generic_tlv_t *tlv = (generic_tlv_t *)buffer;
    assert_int_equal(tlv->TLVType, tlv_friendlyName);
    assert_int_equal(tlv->TLVLength, 0);
}

// Test TLV chain ends with EOP marker
static void test_tlv_chain_ends_with_eop(void **state) {
    (void)state;
    uint8_t buffer[256] = {0};
    size_t offset = 0;

    // Build a minimal TLV chain
    offset += setIconImageTLV(buffer, offset);
    offset += setFriendlyNameTLV(buffer, offset);
    offset += setEndOfPropertyTLV(buffer, offset);

    // Last byte should be EOP marker
    assert_int_equal(buffer[offset - 1], eofpropmarker);
}

// Test generic_tlv_t structure is packed correctly (2 bytes)
static void test_generic_tlv_struct_size(void **state) {
    (void)state;
    // generic_tlv_t should be exactly 2 bytes: 1 byte type + 1 byte length
    assert_int_equal(sizeof(generic_tlv_t), 2);
}

// Test ethernet_address_t is 6 bytes
static void test_ethernet_address_struct_size(void **state) {
    (void)state;
    assert_int_equal(sizeof(ethernet_address_t), 6);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_compare_ethernet_address_equal),
        cmocka_unit_test(test_compare_ethernet_address_not_equal),
        cmocka_unit_test(test_set_lltd_header_populates_fields),
        cmocka_unit_test(test_set_hello_header_writes_payload),
        cmocka_unit_test(test_set_end_of_property_tlv_marks_buffer),
        cmocka_unit_test(test_set_uuid_tlv_correct_size),
        cmocka_unit_test(test_set_perf_counter_tlv_endianness),
        cmocka_unit_test(test_set_qos_characteristics_tlv_endianness),
        cmocka_unit_test(test_set_icon_image_tlv_empty),
        cmocka_unit_test(test_set_friendly_name_tlv_empty),
        cmocka_unit_test(test_tlv_chain_ends_with_eop),
        cmocka_unit_test(test_generic_tlv_struct_size),
        cmocka_unit_test(test_ethernet_address_struct_size),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
