PROGS = librte_pmd_rvif.so

CD := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CC = gcc
PKGCONF = pkg-config

CLEANFILES = $(PROGS) *.o *.d

include $(CD)/mk/dpdk/0.mk
include $(CD)/mk/rvs/0.mk

NO_MAN =
CFLAGS += -O3 -pipe
CFLAGS += -g -rdynamic
CFLAGS += -Werror
CFLAGS += -Wall -Wunused-function
CFLAGS += -Wextra
CFLAGS += -shared -fPIC

CFLAGS += -I$(CD)../include

LDFLAGS +=

C_SRCS = main.c

C_OBJS = $(C_SRCS:.c=.o)

OBJS = $(C_OBJS)

CLEANFILES += $(C_OBJS)

.PHONY: all
all: $(PROGS)

$(DPDK_PKG_CONFIG_FILE):
	make -f $(CD)/mk/dpdk/1.mk

$(OBJS): $(DPDK_PKG_CONFIG_FILE)

$(PROGS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	-@rm -rf $(CLEANFILES)
