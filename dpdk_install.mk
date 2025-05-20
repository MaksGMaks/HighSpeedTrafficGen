DPDK_DIR ?= $(CURDIR)/external/dpdk

.PHONY: get_dpdk remove_external

get_dpdk:
	@echo \"Downloading DPDK to \$(DPDK_DIR)...\"
	mkdir -p $(DPDK_DIR)
	cd $(DPDK_DIR) && wget https://fast.dpdk.org/rel/dpdk-23.11.3.tar.xz
	cd $(DPDK_DIR) && tar -xf dpdk-23.11.3.tar.xz
	@echo \"Building DPDK...\"
	cd $(DPDK_DIR)/dpdk-stable-23.11.3 && meson build
	cd $(DPDK_DIR)/dpdk-stable-23.11.3 && ninja -C build
	@echo \"Loading DPDK into \/usr\/lib...\"
	cd $(DPDK_DIR)/dpdk-stable-23.11.3/build && sudo ninja install
	sudo ldconfig

remove_external:
	rm -rf $(CURDIR)/external

