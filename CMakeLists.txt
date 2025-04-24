cmake_minimum_required(VERSION 3.16)
project(TrafficGenerator)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Attempt to find Qt first
find_package(Qt6 COMPONENTS Core Widgets Gui QUIET)

# If Qt not found, fetch and build it to third-party
if (NOT Qt6_FOUND)
    message(STATUS "Qt not found. Initializing download...")

    include(ExternalProject)
    set(QT_VERSION "6.6.3")
    set(QT_INSTALL_DIR ${CMAKE_SOURCE_DIR}/third-party/qt)

    ExternalProject_Add(
        QtProject
        PREFIX ${CMAKE_BINARY_DIR}/qt_ep
        GIT_REPOSITORY https://code.qt.io/qt/qt5.git
        GIT_TAG v${QT_VERSION}
        CONFIGURE_COMMAND ./configure -prefix ${QT_INSTALL_DIR} -opensource -confirm-license -nomake tests -nomake examples
        BUILD_COMMAND make -j4
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
    )

    # Add Qt install path to CMake search path
    list(APPEND CMAKE_PREFIX_PATH ${QT_INSTALL_DIR})

    # Retry Qt find
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
endif()

message(STATUS "Using Qt version: ${Qt6_VERSION}")

# DPDK
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK QUIET libdpdk)
if (NOT DPDK_FOUND)
    message(STATUS "DPDK not found. Installing...")
    install_package_if_missing("libdpdk-dev" "dpdk-devel" "dpdk")
    pkg_check_modules(DPDK REQUIRED libdpdk)
endif()
include_directories(${DPDK_INCLUDE_DIRS})

# PF_RING

# PF_RING Setup
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
if (EXISTS "${PF_RING_KERNEL_DIR}/lib")
    target_include_directories(${PROJECT_NAME} PRIVATE ${PF_RING_KERNEL_DIR}/include)
    link_directories(${PF_RING_KERNEL_DIR}/lib)
endif()

# Include source directory
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link Qt and DPDK libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${DPDK_LIBRARIES}
)

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
