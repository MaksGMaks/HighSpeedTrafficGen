cmake_minimum_required(VERSION 3.16)
project(TrafficGenerator)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Driver
# option(LOAD_DRIVER "Build and load driver if network device support" OFF)

# Qt auto settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Only build on Linux
if (NOT UNIX)
    message(FATAL_ERROR "This project is only supported on Linux.")
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

# Attempt to find Qt first
find_package(Qt6 COMPONENTS Core Widgets Gui QUIET)

# If Qt not found, fetch and build it to third-party
if (NOT Qt6_FOUND)
    message(STATUS "Installing qt dependencies")
    install_package_if_missing("libgl1-mesa-dev" "mesa-libGL-devel" "mesa")
    install_package_if_missing("libx11-xcb-dev" "libX11-xcb-devel" "libx11")
    install_package_if_missing("libxcb1-dev" "libxcb-devel" "libxcb")
    install_package_if_missing("libglu1-mesa-dev" "mesa-libGLU-devel" "glu")
    install_package_if_missing("libx11-dev" "libX11-devel" "libx11")
    if(NOT Qt6_FOUND)
        message(FATAL_ERROR "Qt6 not found. Please install it from official site: \nhttps://www.qt.io/download-qt-installer-oss \nand set CMAKE_PREFIX_PATH.\nIf Qt installed but you get this error, run CMake with \n-DCMAKE_PREFIX_PATH=/path/to/Qt6/lib/cmake")
    else()
        message(STATUS "Qt found")
    endif()
endif()

message(STATUS "Using Qt version: ${Qt6_VERSION}")

# DPDK
pkg_check_modules(DPDK QUIET libdpdk)
if (NOT DPDK_FOUND)
    message(STATUS "DPDK not found. Installing...")
    install_package_if_missing("libdpdk-dev" "dpdk-devel" "dpdk")
    pkg_check_modules(DPDK REQUIRED libdpdk)
endif()
include_directories(${DPDK_INCLUDE_DIRS})

# PF_RING

# PF_RING Setup
message(STATUS "Checking for PF_RING dependencies...")
install_package_if_missing("libnl-genl-3-dev" "libnl3-devel" "libnl")
install_package_if_missing("bison" "bison" "bison")
install_package_if_missing("flex" "flex" "flex")
install_package_if_missing("gcc-12" "gcc-12" "gcc") 
install_package_if_missing("g++-12" "g++-12" "g++")

set(PF_RING_DIR "${CMAKE_SOURCE_DIR}/external/pf_ring")
set(PF_RING_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/pf_ring")
set(PF_RING_LIBPCAP_DIR "${CMAKE_SOURCE_DIR}/third-party/pf_ring_libpcap")
set(PF_RING_KERNEL_DIR "${CMAKE_SOURCE_DIR}/third-party/pf_ring_kernel")
set(PF_RING_MAKEFILE "${CMAKE_SOURCE_DIR}/pf_ring_install.mk")

if(NOT EXISTS ${PF_RING_LIB_DIR} OR NOT EXISTS ${PF_RING_LIBPCAP_DIR} OR NOT EXISTS ${PF_RING_KERNEL_DIR})
    if(NOT EXISTS "${PF_RING_DIR}")
        message(STATUS "PF_RING not found. Cloning from GitHub...")
        execute_process(
            COMMAND git clone https://github.com/ntop/PF_RING.git ${PF_RING_DIR}
            RESULT_VARIABLE GIT_CLONE_RESULT
        )
        if(NOT GIT_CLONE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to clone PF_RING repository")
        endif()
    endif()
    message(STATUS "Checking PF_RING existing...")
    if(NOT EXISTS ${PF_RING_LIB_DIR})
        message(STATUS "PF_RING not existing. Building LIBPFRING...")
        execute_process(
            COMMAND make -f ${PF_RING_MAKEFILE} libpfring
            RESULT_VARIABLE PF_RING_LIB_MAKE_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT PF_RING_LIB_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build LIBPFRING")
        endif()
    else()
        message(STATUS "PF_RING directory exists, make sure it's not damaged")
    endif()
    message(STATUS "Checking LIBPCAP existing...")
    if(NOT EXISTS ${PF_RING_LIBPCAP_DIR})
        message(STATUS "LIBPCAP not existing. Building LIBPCAP...")
        execute_process(
            COMMAND make -f ${PF_RING_MAKEFILE} libpcap
            RESULT_VARIABLE LIBPCAP_MAKE_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT LIBPCAP_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build LIBPCAP")
        endif()
    else()
        message(STATUS "LIBPCAP directory exists, make sure it's not damaged")
    endif()
    message(STATUS "Checking PF_RING KERNEL existing...")
    if(NOT EXISTS ${PF_RING_KERNEL_DIR})
        message(STATUS "PF_RING KERNEL not existing. Building PF_RING KERNEL...")
        execute_process(
            COMMAND make -f ${PF_RING_MAKEFILE} kernel
            RESULT_VARIABLE PF_RING_KERNEL_MAKE_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT PF_RING_KERNEL_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build PF_RING KERNEL")
        endif()
    else()
        message(STATUS "PF_RING directory exists, make sure it's not damaged")
    endif()
    message(STATUS "Removing external source...")
    execute_process(
        COMMAND make -f ${PF_RING_MAKEFILE} remove_external
        RESULT_VARIABLE REMOVE_PF_RING_MAKE_RESULT
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    if(NOT REMOVE_PF_RING_MAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clean external directory")
    endif()
endif()

# Project sources
set(ALL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Generator/Generator.cpp
)

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# Add PF_RING if available
if (EXISTS "${PF_RING_LIB_DIR}/lib")
    target_include_directories(${PROJECT_NAME} PRIVATE ${PF_RING_LIB_DIR}/include)
    link_directories(${PF_RING_LIB_DIR}/lib)
endif()
if (EXISTS "${PF_RING_LIBPCAP_DIR}/lib")
    target_include_directories(${PROJECT_NAME} PRIVATE ${PF_RING_LIBPCAP_DIR}/include)
    link_directories(${PF_RING_LIBPCAP_DIR}/lib)
endif()
if (EXISTS "${PF_RING_KERNEL_DIR}/module")
    target_include_directories(${PROJECT_NAME} PRIVATE ${PF_RING_KERNEL_DIR}/include)
    link_directories(${PF_RING_KERNEL_DIR}/module)
endif()

# Include source directory
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link Qt and DPDK libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${DPDK_LIBRARIES}
    ${PF_RING_LIB_DIR}/lib/libpfring.so.9.1.0
    ${PF_RING_LIBPCAP_DIR}/lib/libpcap.so.1.10.1
)

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# if(LOAD_DRIVER)
#     message(STATUS "Checking if network device support driver...")
#     DRIVER_CMAKE=$(make -f ${PF_RING_MAKEFILE} get_driver)
#     set(MY_LIST "e1000e" "i40e" "iavf" "ice" "igb" "irdma-*" "ixgbe" "ixgbevf" "pfring-drivers-zc-dkms" zc-dkms-mkdeb")
# 
#     list(FIND MY_LIST "${DRIVER_CMAKE}" _index)
# 
#     if(NOT _index EQUAL -1)        
#         message(STATUS "Driver found. Initiate building...")
#         if(NOT EXISTS "${PF_RING_DIR}")
#             message(STATUS "PF_RING not found. Cloning from GitHub...")
#             execute_process(
#                 COMMAND git clone https://github.com/ntop/PF_RING.git ${PF_RING_DIR}
#                 RESULT_VARIABLE GIT_CLONE_RESULT
#             )
#             if(NOT GIT_CLONE_RESULT EQUAL 0)
#                 message(FATAL_ERROR "Failed to clone PF_RING repository")
#             endif()
#         endif()
#         execute_process(
#             COMMAND make -f ${PF_RING_MAKEFILE} build_driver DRIVER=${DRIVER_CMAKE}
#             RESULT_VARIABLE DRIVER_BUILD_MAKE
#             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#         )
#         if(NOT DRIVER_BUILD_MAKE 0)
#             message(FATAL_ERROR "Failed to build LIBPFRING")
#         endif()
#     else()
#         message(STATUS "Driver is NOT found.")
#     endif()
# endif()
