#!/usr/bin/make -f

CC     := gcc
CFLAGS += -Wall -Wextra

TEST_DIR := build/tests
TEST_CFLAGS := $(CFLAGS) -IlltdDaemon -DLLTD_TESTING
TEST_LDFLAGS :=

ifneq ($(SANITIZE),)
	TEST_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g
	TEST_LDFLAGS += -fsanitize=address,undefined
endif

ifneq ($(COVERAGE),)
	TEST_CFLAGS += --coverage -O0 -g
	TEST_LDFLAGS += --coverage
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CCFLAGS += -D LINUX
#	LLTD_SRC_FILES = lltdDaemon/linux/linux-main.c lltdDaemon/linux/linux-ops.c lltdDaemon/lltdBlock.c lltdDaemon/lltdTlvOps.c
	LLTD_SRC_FILES = lltdDaemon/linux/linux-main.c
endif
ifeq ($(UNAME_S),Darwin)
	CCFLAGS += -D OSX
endif
ifeq ($(UNAME_S),SunOS)
	CCFLAGS += -D SunOS
endif
ifeq ($(UNAME_S),FreeBSD)
	CCFLAGS += -D FreeBSD
endif

UNAME_P := $(shell uname -p)
ifeq ($(UNAME_P),x86_64)
	CCFLAGS += -D AMD64
endif
ifeq ($(UNAME_P),amd64)
	CCFLAGS += -D AMD64
endif
ifeq ($(UNAME_P),sparc)
	CCFLAGS += -D SPARC
endif
ifeq ($(UNAME_P),sparc64)
	CCFLAGS += -D AMD64
endif
ifeq ($(UNAME_P),mips)
	CCFLAGS += -D MIPS
endif
ifeq ($(UNAME_P),ppc)
	CCFLAGS += -D PPC
endif
ifeq ($(UNAME_P),powerpc)
	CCFLAGS += -D PPC
endif
ifeq ($(UNAME_P),ppc64)
	CCFLAGS += -D PPC
endif
ifneq ($(filter %86,$(UNAME_P)),)
	CCFLAGS += -D IA32
endif
ifneq ($(filter arm%,$(UNAME_P)),)
	CCFLAGS += -D ARM
endif

all: lltdResponder

lltdResponder: $(LLTD_SRC_FILES)
	gcc -o lltdReponder $(LLTD_SRC_FILES)

test-check:
	@pkg-config --exists cmocka || (echo "cmocka not found (pkg-config cmocka). Install cmocka to run tests." >&2; exit 1)

$(TEST_DIR):
	mkdir -p $(TEST_DIR)

test-unit: test-check $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) $$(pkg-config --cflags cmocka) \
		lltdDaemon/lltdTlvOps.c tests/test_shims.c tests/test_lltd_tlv_ops.c \
		$(TEST_LDFLAGS) $$(pkg-config --libs cmocka) -o $(TEST_DIR)/unit_tests
	$(TEST_DIR)/unit_tests

test-integration: test-check $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) $$(pkg-config --cflags cmocka) \
		lltdDaemon/lltdTlvOps.c tests/test_shims.c tests/test_lltd_integration.c \
		$(TEST_LDFLAGS) $$(pkg-config --libs cmocka) -o $(TEST_DIR)/integration_tests
	$(TEST_DIR)/integration_tests

test-privileged: test-check $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) $$(pkg-config --cflags cmocka) \
		tests/test_shims.c tests/test_lltd_privileged.c \
		$(TEST_LDFLAGS) $$(pkg-config --libs cmocka) -o $(TEST_DIR)/privileged_tests
	$(TEST_DIR)/privileged_tests

test: test-unit test-integration

coverage: COVERAGE=1
coverage: test

clean-tests:
	-rm -rf $(TEST_DIR)
	-rm -f *.gcda *.gcno *.gcov lltdDaemon/*.gcda lltdDaemon/*.gcno lltdDaemon/*.gcov tests/*.gcda tests/*.gcno tests/*.gcov

clean:
	-rm -f lltdResponder
	-rm -rf $(TEST_DIR)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: all linux clean test test-unit test-integration test-privileged test-check coverage clean-tests
