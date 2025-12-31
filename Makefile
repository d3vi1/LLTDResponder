#!/usr/bin/make -f

CC     := gcc
CFLAGS += -Wall -Wextra

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


clean:
	-rm -f lltdResponder

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: all linux clean
