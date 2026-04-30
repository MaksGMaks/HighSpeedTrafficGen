MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)
BOOST_DIR ?= $(ROOT_DIR)/third-party

.PHONY: get_boost_lin

get_boost_lin:
	@echo \"[*] Downloading BOOST to \$(BOOST_DIR)...\"
	mkdir -p $(BOOST_DIR)
	cd $(BOOST_DIR) && wget https://archives.boost.io/release/1.88.0/source/boost_1_88_0.tar.gz
	cd $(BOOST_DIR) && tar -xf boost_1_88_0.tar.gz
	cd $(BOOST_DIR) && mv ./boost_1_88_0 ./boost 
	@echo \"[*] Cleaning downloads...\"
	cd $(BOOST_DIR) && rm boost_1_88_0.tar.gz



