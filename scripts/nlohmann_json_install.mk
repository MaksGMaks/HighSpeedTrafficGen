MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)

NLOHMANN_DIR         ?= $(ROOT_DIR)/external/nlohmann_json
NLOHMANN_INSTALL_DIR ?= $(ROOT_DIR)/third-party/nlohmann_json

.PHONY: get_nlohmann remove_nlohmann

get_nlohmann:
	@echo "Copying nlohmann/json headers..."
	mkdir -p $(NLOHMANN_INSTALL_DIR)
	cp -r $(NLOHMANN_DIR)/include/. $(NLOHMANN_INSTALL_DIR)/
	@echo "nlohmann/json headers copied."

remove_external:
	rm -rf $(NLOHMANN_DIR)