RVS_DIR := $(CD)/deps/rvs

CFLAGS += -I$(RVS_DIR)/include

all: $(RVS_DIR)/include/rvif.h

$(RVS_DIR)/include/rvif.h:
	mkdir -p $(CD)/deps
	git -C $(CD)/deps clone https://github.com/yasukata/rvs.git
