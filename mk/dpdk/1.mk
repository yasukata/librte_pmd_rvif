CD := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../../

include  $(CD)/mk/dpdk/0.mk

all: $(DPDK_PKG_CONFIG_FILE)

$(DPDK_SRC_DIR).tar.xz:
	wget -P $(DPDK_DIR) https://fast.dpdk.org/rel/dpdk-$(DPDK_VER).tar.xz

$(DPDK_SRC_DIR): $(DPDK_SRC_DIR).tar.xz
	mkdir -p $(DPDK_SRC_DIR)
	tar xvf $< -C $(DPDK_SRC_DIR) --strip-components=1

$(DPDK_PKG_CONFIG_FILE): $(DPDK_SRC_DIR)
	meson --default-library=shared --prefix=$(DPDK_INSTALL_DIR) --libdir=lib/x86_64-linux-gnu $(DPDK_SRC_DIR)/build $(DPDK_SRC_DIR)
	ninja -C $(DPDK_SRC_DIR)/build
	ninja -C $(DPDK_SRC_DIR)/build install
