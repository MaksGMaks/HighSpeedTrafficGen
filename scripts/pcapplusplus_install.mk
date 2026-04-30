MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR := $(abspath $(MAKEFILE_DIR)/..)

PCAPPP_DIR ?= $(ROOT_DIR)/external/PcapPlusPlus
PCAPPP_INSTALL_DIR ?= $(ROOT_DIR)/third-party/pcapplusplus

# Extract -j N from MAKEFLAGS if present
MAKEFLAGS_J := $(patsubst -j%,%,$(filter -j%,$(MAKEFLAGS)))

# Build the parallel flag for cmake: "--parallel N" if -j N given, just "--parallel" otherwise
ifneq ($(MAKEFLAGS_J),)
    CMAKE_PARALLEL := --parallel $(MAKEFLAGS_J)
else
    CMAKE_PARALLEL := --parallel
endif

.PHONY: get_pcapplusplus remove_external

get_pcapplusplus:
	@echo "Building PcapPlusPlus..."
	cmake -S $(PCAPPP_DIR) -B $(PCAPPP_DIR)/build \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(PCAPPP_INSTALL_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		-DPCAPPP_BUILD_EXAMPLES=OFF \
		-DPCAPPP_BUILD_TESTS=OFF \
		-DPCAPPP_INSTALL=ON
	cmake --build $(PCAPPP_DIR)/build $(CMAKE_PARALLEL)
	@echo "Copying PcapPlusPlus build to $(PCAPPP_INSTALL_DIR)..."
	mkdir -p $(PCAPPP_INSTALL_DIR)/lib
	mkdir -p $(PCAPPP_INSTALL_DIR)/include
	cmake --install $(PCAPPP_DIR)/build
	@echo "PcapPlusPlus build complete."

remove_external:
	rm -rf $(ROOT_DIR)/external