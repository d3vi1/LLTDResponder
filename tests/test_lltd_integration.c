#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "lltdTestShim.h"
#include "lltdEndian.h"
#include "lltdTlvOps.h"

static void test_set_ipv4_tlv_uses_interface(void **state) {
    (void)state;
    lltd_test_iface_t iface;
    memset(&iface, 0, sizeof(iface));
    iface.ipv4_be = lltd_htonl(0x01020304);
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint32_t)] = {0};

    size_t written = setIPv4TLV(buffer, 0, &iface);

    assert_int_equal(written, sizeof(generic_tlv_t) + sizeof(uint32_t));
    generic_tlv_t *hdr = (generic_tlv_t *)buffer;
    assert_int_equal(hdr->TLVType, tlv_ipv4);
    assert_int_equal(hdr->TLVLength, sizeof(uint32_t));

    const uint8_t *payload = buffer + sizeof(*hdr);
    assert_int_equal(payload[0], 0x01);
    assert_int_equal(payload[1], 0x02);
    assert_int_equal(payload[2], 0x03);
    assert_int_equal(payload[3], 0x04);
}

static void test_set_ipv6_tlv_uses_interface(void **state) {
    (void)state;
    lltd_test_iface_t iface;
    memset(&iface, 0, sizeof(iface));
    for (size_t i = 0; i < sizeof(iface.ipv6); i++) {
        iface.ipv6[i] = (uint8_t)(i + 1);
    }
    uint8_t buffer[sizeof(generic_tlv_t) + 16] = {0};

    size_t written = setIPv6TLV(buffer, 0, &iface);

    assert_int_equal(written, sizeof(generic_tlv_t) + 16);
    generic_tlv_t *hdr = (generic_tlv_t *)buffer;
    assert_int_equal(hdr->TLVType, tlv_ipv6);
    assert_int_equal(hdr->TLVLength, 16);

    assert_memory_equal(buffer + sizeof(*hdr), iface.ipv6, 16);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_set_ipv4_tlv_uses_interface),
        cmocka_unit_test(test_set_ipv6_tlv_uses_interface),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
