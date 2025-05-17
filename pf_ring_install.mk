PF_RING_DIR ?= $(CURDIR)/external/pf_ring

USERLAND_DIR := $(PF_RING_DIR)/userland
PF_RING_BUILD_DIR := $(USERLAND_DIR)/lib
LIBPCAP_BUILD_DIR := $(PF_RING_DIR)/userland/libpcap

INSTALL_DIR ?= $(CURDIR)/third-party
PF_RING_INSTALL_DIR := $(INSTALL_DIR)/pf_ring
LIBPCAP_INSTALL_DIR := $(INSTALL_DIR)/pf_ring_libpcap
PF_RING_KERNEL_INSTALL_DIR := $(INSTALL_DIR)/pf_ring_kernel

DRIVERS_DIR := $(PF_RING_DIR)/drivers/intel
AVAILABLE_DRIVERS := e1000e i40e iavf ice igb irdma-* ixgbe ixgbevf pfring-drivers-zc-dkms zc-dkms-mkdeb

.PHONY:  clean_kernel clean_libpcap clean_libpfring kernel libpcap libpfring remove_external
# .PHONY: build_driver get_driver
libpfring:
	@echo \"Building Libpfring...\"
	cd \$(PF_RING_BUILD_DIR) && ./configure && make
	@echo \"Copying Libpfring built to \$(PF_RING_INSTALL_DIR)...\"
	mkdir -p \$(PF_RING_INSTALL_DIR)/lib
	mkdir -p \$(PF_RING_INSTALL_DIR)/include
	cp \$(PF_RING_BUILD_DIR)/libpfring.so* \$(PF_RING_INSTALL_DIR)/lib/ || true
	@for file in $(PF_RING_BUILD_DIR)/*.h; do \
		if [ -f $$file ]; then \
			cp $$file $(PF_RING_INSTALL_DIR)/include/; \
		fi \
	done
	sed -i 's|#include <linux/pf_ring.h>|#include "../../pf_ring_kernel/include/pf_ring.h"|' $(PF_RING_INSTALL_DIR)/include/pfring.h
	sed -i 's|#include <linux/pf_ring.h>|#include "../../pf_ring_kernel/include/pf_ring.h"|' $(PF_RING_INSTALL_DIR)/include/pfring_zc.h
	sed -i 's|char[[:space:]]*\*slots;|char *dev_slots;|' $(PF_RING_INSTALL_DIR)/include/pfring.h
	@echo \"Loading created library into \/usr\/lib\"
	sudo cp \$(PF_RING_BUILD_DIR)/libpfring.so* /usr/lib/
	sudo ldconfig

libpcap:
	@echo \"Building Libpfring...\"
	cd \$(LIBPCAP_BUILD_DIR) && ./configure && make
	@echo \"Copying Libpfring built to \$(LIBPCAP_INSTALL_DIR)...\"
	mkdir -p \$(LIBPCAP_INSTALL_DIR)/lib
	mkdir -p \$(LIBPCAP_INSTALL_DIR)/include
	cp \$(LIBPCAP_BUILD_DIR)/libpcap.so* \$(LIBPCAP_INSTALL_DIR)/lib/ || true
	@for file in $(LIBPCAP_BUILD_DIR)/*.h; do \
		if [ -f $$file ]; then \
			cp $$file $(LIBPCAP_INSTALL_DIR)/include/; \
		fi \
	done
	@echo \"Loading created library into \/usr\/lib\"
	sudo cp \$(LIBPCAP_BUILD_DIR)/libpcap.so* /usr/lib/
	sudo ldconfig

kernel:
	@echo \"Building PF_RING Kernel...\"
	cd $(PF_RING_DIR)/kernel && make
	@mkdir -p $(PF_RING_KERNEL_INSTALL_DIR)/module
	@mkdir -p $(PF_RING_KERNEL_INSTALL_DIR)/include
	cp $(PF_RING_DIR)/kernel/pf_ring.ko $(PF_RING_KERNEL_INSTALL_DIR)/module
	cp $(PF_RING_DIR)/kernel/linux/pf_ring.h $(PF_RING_KERNEL_INSTALL_DIR)/include/pf_ring.h

remove_external:
	rm -rf $(CURDIR)/external

clean_libpfring:
	@echo \"Cleaning PF_RING build...\"
	rm -rf \$(PF_RING_INSTALL_DIR)

clean_libpcap:
	@echo \"Cleaning libpcap build...\"
	rm -rf \$(LIBPCAP_INSTALL_DIR)

clean_kernel:
	@echo \"Cleaning kernel build...\"
	rm -rf \$(PF_RING_KERNEL_INSTALL_DIR)

build_driver:
	@echo "[*] Building driver: $(DRIVER)"
	cd $(DRIVERS_DIR) && ./configure
	@interfaces=$$(ls /sys/class/net | grep -v '^lo$$'); \
	for iface in $$interfaces; do \
		driver=$$(ethtool -i $$iface | grep driver | awk '{print $$2}'); \
		if echo "$(AVAILABLE_DRIVERS)" | grep -qw "$$driver"; then \
			echo "[DRIVER] $$iface has PF_RING ZC driver ($$driver). Initiate building..."; \
			cd $(DRIVERS_DIR)/$$driver/$$driver-*-zc && make; \
			cd $(DRIVERS_DIR)/$$driver/$$driver-*-zc/src && sudo ./load_driver.sh; \
		else \
			echo "[DRIVER] $$iface has not PF_RING ZC driver. Skipping..."; \
		fi; \
	done