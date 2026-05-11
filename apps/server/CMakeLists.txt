cmake_minimum_required(VERSION 3.21)
project(HSET_GeneratorServer)

set(SERVER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SERVER_BLD "${CMAKE_BINARY_DIR}/server")

# Driver
option(LOAD_DRIVER "Build and load driver if network device support" OFF)

if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  set(N $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
else()
  set(N 0)
endif()

# Only build on Linux
if (NOT UNIX)
  message(FATAL_ERROR "Server project is only supported on Linux.")
endif()

# Function to install packages based on distro
function(install_package_if_missing debian_name fedora_name arch_name)
  execute_process(
    COMMAND bash -c "
    if [ -f /etc/debian_version ]; then
      if dpkg -s ${debian_name} 2>/dev/null | grep -q 'Status: install ok installed'; then
        echo '[INFO] ${debian_name} already installed.';
      else
        echo '[INFO] Attempting to install ${debian_name}...';
        sudo apt update && sudo apt install -y ${debian_name};
      fi
    elif [ -f /etc/fedora-release ]; then
      if rpm -q ${fedora_name} &>/dev/null; then
        echo '[INFO] ${fedora_name} already installed.';
      else
        echo '[INFO] Attempting to install ${fedora_name}...';
        sudo dnf install -y ${fedora_name};
      fi
    elif [ -f /etc/arch-release ]; then
      if pacman -Q ${arch_name} &>/dev/null; then
        echo '[INFO] ${arch_name} already installed.';
      else
        echo '[INFO] Attempting to install ${arch_name}...';
        sudo pacman -Sy --noconfirm ${arch_name};
      fi
    else
      echo '[ERROR] Unknown distro. Install manually.';
      exit 1;
    fi"
    RESULT_VARIABLE result
  )
  if (NOT result EQUAL 0)
      message(FATAL_ERROR "Failed to install (${debian_name} / ${fedora_name} / ${arch_name}).")
  endif()
endfunction()

# Find PkgConfig for dependencies
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
  message(STATUS "PkgConfig found: ${PKG_CONFIG_EXECUTABLE}")
else()
  message(Status "PkgConfig NOT found. Installing:")
    install_package_if_missing("pkg-config" "pkgconf-pkg-config" "pkgconf")
endif()

set(SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/scripts")

# DPDK
set(DPDK_MAKEFILE "${SCRIPTS_DIR}/dpdk_install.mk")
message(STATUS "Checking for DPDK...")
pkg_check_modules(DPDK QUIET libdpdk)
if (NOT DPDK_FOUND)
  message(STATUS "DPDK not found. Installing...")
  message(STATUS "Installing DPDK dependencies...")

  install_package_if_missing("build-essential" "@development-tools" "base-devel")
  install_package_if_missing("meson" "meson" "meson")
  install_package_if_missing("python3-pyelftools" "python3-pyelftools" "python-pyelftools")
  install_package_if_missing("libnuma-dev" "numactl-devel" "numactl")
  install_package_if_missing("pkgconf" "pkgconf" "pkgconf")
  install_package_if_missing("ninja-build" "ninja-build" "ninja")

  if(N GREATER 0)
    execute_process(
      COMMAND make -j ${N} -f ${DPDK_MAKEFILE} get_dpdk
      RESULT_VARIABLE DPDK_LIB_MAKE_RESULT
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  else()
    execute_process(
      COMMAND make -f ${DPDK_MAKEFILE} get_dpdk
      RESULT_VARIABLE DPDK_LIB_MAKE_RESULT
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endif()
  if(NOT DPDK_LIB_MAKE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build DPDK")
  endif()
  pkg_check_modules(DPDK REQUIRED libdpdk)
  message(STATUS "Removing external source...")
  execute_process(
    COMMAND make -f ${DPDK_MAKEFILE} remove_external
    RESULT_VARIABLE REMOVE_DPDK_MAKE_RESULT
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  if(NOT REMOVE_DPDK_MAKE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to clean external directory")
  endif()
endif()
include_directories(${DPDK_INCLUDE_DIRS})
message(STATUS "DPDK found")

# Project sources
set(ALL_SOURCES
#  ${SERVER_SRC}/Generator/common_generator.cpp
#  ${SERVER_SRC}/Generator/DPDK_Engine.cpp
#  ${SERVER_SRC}/Generator/Generator.cpp
#  ${SERVER_SRC}/Generator/PFRING_Engine.cpp
#  ${SERVER_SRC}/Network/NetworkManager.cpp
#  ${SERVER_SRC}/UIUpdate/UIUpdater.cpp
)

# Main target
add_executable(${PROJECT_NAME} 
  ${ALL_SOURCES}
  ${SERVER_SRC}/main.cpp
)

# Add Boost
set(BOOST_DIR "${CMAKE_SOURCE_DIR}/third-party/boost")
if(EXISTS ${BOOST_DIR})
  target_include_directories(${PROJECT_NAME} PRIVATE ${BOOST_DIR})
#  target_include_directories(${PROJECT_NAME}Manager PRIVATE ${BOOST_DIR})
endif()

set(PCAPPP_DIR "${CMAKE_SOURCE_DIR}/third-party/pcapplusplus")
if(EXISTS ${PCAPPP_DIR})
  target_include_directories(${PROJECT_NAME} PRIVATE "${PCAPPP_DIR}/pcapplusplus/include ")
  #  target_include_directories(${PROJECT_NAME}Manager PRIVATE ${PCAPPP_DIR})

  find_library(PCAPPP_LIB    Pcap++   PATHS ${PCAPPP_DIR}/lib NO_DEFAULT_PATH)
  find_library(PACKETPP_LIB  Packet++ PATHS ${PCAPPP_DIR}/lib NO_DEFAULT_PATH)
  find_library(COMMONPP_LIB  Common++ PATHS ${PCAPPP_DIR}/lib NO_DEFAULT_PATH)

  if(PCAPPP_LIB AND PACKETPP_LIB AND COMMONPP_LIB)
    message(STATUS "PcapPlusPlus found: ${PCAPPP_DIR}")
  else()
    message(FATAL_ERROR "PcapPlusPlus libraries not found in ${PCAPPP_DIR}/lib")
  endif()
else()
  message(FATAL_ERROR "PcapPlusPlus not found at ${PCAPPP_DIR}")
endif()

# Include source directory
target_include_directories(${PROJECT_NAME} PUBLIC ${SERVER_SRC})

# Compile options
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
  target_compile_options(${PROJECT_NAME} PRIVATE -mssse3)
endif()

# Link Qt and DPDK libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  CommonLibrary
  ${DPDK_LIBRARIES}
  ${PCAPPP_LIB}
  ${PACKETPP_LIB}
  ${COMMONPP_LIB}
  pcap      # системна libpcap
  pthread
  dl
)

# Set the destination directory for the executable
set_target_properties(${PROJECT_NAME} PROPERTIES 
  RUNTIME_OUTPUT_DIRECTORY ${SERVER_BLD}
)
