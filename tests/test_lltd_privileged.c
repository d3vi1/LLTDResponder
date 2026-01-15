#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>

#include <cmocka.h>

#include "lltdTestShim.h"

#ifdef __linux__
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "lltdEndian.h"

static void test_raw_socket_open(void **state) {
    (void)state;
#ifdef __linux__
    int fd = socket(AF_PACKET, SOCK_RAW, lltd_htons(lltdEtherType));
    if (fd < 0) {
        if (errno == EPERM || errno == EACCES) {
            print_message("Raw socket unavailable without CAP_NET_RAW; skipping.\n");
            return;
        }
    }
    assert_true(fd >= 0);
    if (fd >= 0) {
        close(fd);
    }
#else
    print_message("Raw socket test is Linux-only; skipping.\n");
#endif
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_raw_socket_open),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
