#include <ifaddrs.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "lltdTestShim.h"
#include "lltdTlvOps.h"

static char *find_ipv4_interface(void) {
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) {
        return NULL;
    }

    char *name = NULL;
    for (struct ifaddrs *cursor = interfaces; cursor != NULL; cursor = cursor->ifa_next) {
        if (!cursor->ifa_addr) {
            continue;
        }
        if (cursor->ifa_addr->sa_family == AF_INET) {
            name = strdup(cursor->ifa_name);
            break;
        }
    }

    freeifaddrs(interfaces);
    return name;
}

static void test_set_ipv4_tlv_uses_interface(void **state) {
    (void)state;
    char *iface_name = find_ipv4_interface();
    if (!iface_name) {
        print_message("No IPv4 interface found; skipping integration test.\n");
        return;
    }

    network_interface_t iface = {
        .deviceName = iface_name,
    };
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(uint32_t)] = {0};

    size_t written = setIPv4TLV(buffer, 0, &iface);

    assert_int_equal(written, sizeof(generic_tlv_t) + sizeof(uint32_t));
    generic_tlv_t *hdr = (generic_tlv_t *)buffer;
    assert_int_equal(hdr->TLVType, tlv_ipv4);
    assert_int_equal(hdr->TLVLength, sizeof(uint32_t));

    free(iface_name);
}

static void test_set_ipv6_tlv_uses_interface(void **state) {
    (void)state;
    char *iface_name = find_ipv4_interface();
    if (!iface_name) {
        print_message("No IPv4 interface found; skipping IPv6 integration test.\n");
        return;
    }

    network_interface_t iface = {
        .deviceName = iface_name,
    };
    uint8_t buffer[sizeof(generic_tlv_t) + sizeof(struct in6_addr)] = {0};

    size_t written = setIPv6TLV(buffer, 0, &iface);

    assert_int_equal(written, sizeof(generic_tlv_t) + sizeof(struct in6_addr));
    generic_tlv_t *hdr = (generic_tlv_t *)buffer;
    assert_int_equal(hdr->TLVType, tlv_ipv6);
    assert_int_equal(hdr->TLVLength, sizeof(struct in6_addr));

    free(iface_name);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_set_ipv4_tlv_uses_interface),
        cmocka_unit_test(test_set_ipv6_tlv_uses_interface),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
