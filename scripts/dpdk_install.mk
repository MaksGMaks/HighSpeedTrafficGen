MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)

DPDK_DIR ?= $(ROOT_DIR)/external/dpdk

.PHONY: get_dpdk remove_external

get_dpdk:
	@echo \"Downloading DPDK to \$(DPDK_DIR)...\"
	mkdir -p $(DPDK_DIR)
	cd $(DPDK_DIR) && wget https://fast.dpdk.org/rel/dpdk-26.03.tar.xz
	cd $(DPDK_DIR) && tar -xf dpdk-26.03.tar.xz
	@echo \"Building DPDK...\"
	cd $(DPDK_DIR)/dpdk-26.03 && meson build --buildtype=release
	cd $(DPDK_DIR)/dpdk-26.03 && ninja -C build
	@echo \"Loading DPDK into \/usr\/lib...\"
	cd $(DPDK_DIR)/dpdk-26.03/build && sudo ninja install
	sudo ldconfig

remove_external:
	rm -rf $(DPDK_DIR)
