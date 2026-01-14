#!/usr/bin/make -f

CC     ?= gcc
CFLAGS += -Wall -Wextra

BIN_NAME ?= build/lltdResponder

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
	CFLAGS += -D LINUX
	PLATFORM ?= linux-systemd
	ifeq ($(PLATFORM),linux-systemd)
		LLTD_SRC_FILES = lltdDaemon/linux/linux-main.c \
			lltdDaemon/linux/linux-ops.c \
			lltdDaemon/lltdBlock.c \
			lltdDaemon/lltdTlvOps.c \
			lltdResponder/lltdAutomata.c \
			os/linux/lltd_port.c
		LLTD_CFLAGS += -DLLTD_BACKEND_SYSTEMD -DLLTD_USE_SYSTEMD
		LLTD_LDFLAGS += -lsystemd
	else ifeq ($(PLATFORM),linux-embedded)
		LLTD_SRC_FILES = lltdDaemon/linux/linux-embedded-main.c \
			lltdDaemon/linux/linux-ops.c \
			lltdDaemon/lltdBlock.c \
			lltdDaemon/lltdTlvOps.c \
			lltdResponder/lltdAutomata.c \
			os/linux/lltd_port.c
		LLTD_CFLAGS += -DLLTD_BACKEND_EMBEDDED -DLLTD_USE_CONSOLE
	else
		$(error Unsupported PLATFORM '$(PLATFORM)'; use linux-systemd or linux-embedded)
	endif
endif
ifeq ($(UNAME_S),Darwin)
	CFLAGS += -D OSX
endif
ifeq ($(UNAME_S),SunOS)
	CFLAGS += -D SunOS
endif
ifeq ($(UNAME_S),FreeBSD)
	CFLAGS += -D FreeBSD
endif

UNAME_P := $(shell uname -p)
ifeq ($(UNAME_P),x86_64)
	CFLAGS += -D AMD64
endif
ifeq ($(UNAME_P),amd64)
	CFLAGS += -D AMD64
endif
ifeq ($(UNAME_P),sparc)
	CFLAGS += -D SPARC
endif
ifeq ($(UNAME_P),sparc64)
	CFLAGS += -D AMD64
endif
ifeq ($(UNAME_P),mips)
	CFLAGS += -D MIPS
endif
ifeq ($(UNAME_P),ppc)
	CFLAGS += -D PPC
endif
ifeq ($(UNAME_P),powerpc)
	CFLAGS += -D PPC
endif
ifeq ($(UNAME_P),ppc64)
	CFLAGS += -D PPC
endif
ifneq ($(filter %86,$(UNAME_P)),)
	CFLAGS += -D IA32
endif
ifneq ($(filter arm%,$(UNAME_P)),)
	CFLAGS += -D ARM
endif

all: $(BIN_NAME)

$(BIN_NAME): $(LLTD_SRC_FILES)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LLTD_CFLAGS) -o $@ $(LLTD_SRC_FILES) $(LLTD_LDFLAGS)

$(TEST_DIR):
	mkdir -p $(TEST_DIR)

TEST_SOURCES = lltdDaemon/lltdTlvOps.c tests/test_shims.c tests/test_lltd_tlv_ops.c

test-unit: $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) $(TEST_SOURCES) \
		$(TEST_LDFLAGS) -o $(TEST_DIR)/unit_tests
	$(TEST_DIR)/unit_tests

test: test-unit

integration-helper: $(TEST_DIR)
	$(CC) $(TEST_CFLAGS) lltdDaemon/lltdTlvOps.c tests/test_shims.c \
		tests/integration/lltd_integration_smoke.c \
		$(TEST_LDFLAGS) -o $(TEST_DIR)/lltd_integration_smoke

integration-test: integration-helper
	./tests/integration/linux_veth.sh $(TEST_DIR)/lltd_integration_smoke ./$(BIN_NAME)

coverage: COVERAGE=1
coverage: test

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
	-rm -f $(BIN_NAME)
	-rm -rf $(TEST_DIR)

win9x-vxd:
	./scripts/build-win9x-vxd.sh
	./scripts/gen-inf.py

win16:
	./scripts/build-win16.sh
	./scripts/gen-inf.py

windows-infs:
	./scripts/gen-inf.py

windows-openwatcom: win16 win9x-vxd

macos-universal:
	./scripts/macos-build.sh

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: all linux clean test test-unit test-integration test-privileged test-check coverage clean-tests win9x-vxd win16 windows-infs windows-openwatcom macos-universal
