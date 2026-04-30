cmake_minimum_required(VERSION 3.21)
project(HSET_Generator)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LSP server
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_CLIENT "Build the client application." ON)
option(BUILD_SERVER "Build the server application." OFF)

if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(N $ENV{CMAKE_BUILD_PARALLEL_LEVEL})
else()
    set(N 0)
endif()

set(SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/scripts")

# Third-parties libs
set(BOOST_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/boost")
set(BOOST_MAKEFILE "${SCRIPTS_DIR}/boost_install.mk")
set(BOOST_BAT "${SCRIPTS_DIR}/boost_install.bat")

set(NLOHMANNJSON_DIR "${CMAKE_SOURCE_DIR}/external/nlohmann_json")
set(NLOHMANNJSON_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/nlohmann_json")
set(NLOHMANNJSON_MAKEFILE "${SCRIPTS_DIR}/nlohmann_json_install.mk")
set(NLOHMANNJSON_BAT "${SCRIPTS_DIR}/nlohmann_json_install.bat")

set(PCAPPP_DIR "${CMAKE_SOURCE_DIR}/external/PcapPlusPlus")
set(PCAPPP_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/pcapplusplus")
set(PCAPPP_MAKEFILE "${SCRIPTS_DIR}/pcapplusplus_install.mk")
set(PCAPPP_BAT "${SCRIPTS_DIR}/pcapplusplus_install.bat")


if (UNIX)
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

    message(STATUS "Checking for boost...")
    if(NOT EXISTS ${BOOST_LIB_DIR})
        message(STATUS "Boost not found. Preparing script to install to third-party...")
        if(N GREATER 0)
            execute_process(
                    COMMAND make -j ${N} -f ${BOOST_MAKEFILE} get_boost_lin
                    RESULT_VARIABLE BOOST_LIB_MAKE_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        else()
            execute_process(
                    COMMAND make -f ${BOOST_MAKEFILE} get_boost_lin
                    RESULT_VARIABLE BOOST_LIB_MAKE_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endif()
        if(NOT BOOST_LIB_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to install BOOST")
        else()
            message(Status "Installation completed")
        endif()
    else()
        message(STATUS "Boost installed. Make sure it's not damaged")
    endif()

    message(STATUS "Checking for nlohmann/json...")
    if(NOT EXISTS ${NLOHMANNJSON_LIB_DIR})
        if(NOT EXISTS "${NLOHMANNJSON_DIR}")
            message(STATUS "nlohmann/json not found. Cloning from GitHub...")
            execute_process(
                    COMMAND git clone --depth 1 --branch v3.11.3
                    https://github.com/nlohmann/json.git
                    ${NLOHMANNJSON_DIR}
                    RESULT_VARIABLE GIT_CLONE_RESULT
            )
            if(NOT GIT_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone nlohmann/json repository")
            endif()
        endif()
        message(STATUS "nlohmann/json not found. Preparing script to install to third-party...")
        if(N GREATER 0)
            execute_process(
                    COMMAND make -j ${N} -f ${NLOHMANNJSON_MAKEFILE} get_nlohmann
                    RESULT_VARIABLE NLOHMANNJSON_LIB_MAKE_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        else()
            execute_process(
                    COMMAND make -f ${NLOHMANNJSON_MAKEFILE} get_nlohmann
                    RESULT_VARIABLE NLOHMANNJSON_LIB_MAKE_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endif()
        if(NOT NLOHMANNJSON_LIB_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to install nlohmann/json")
        else()
            message(Status "Installation completed")
        endif()
        message(STATUS "Removing external source...")
        execute_process(
                COMMAND make -f ${NLOHMANNJSON_MAKEFILE} remove_external
                RESULT_VARIABLE REMOVE_NLOHMANNJSON_MAKE_RESULT
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT REMOVE_NLOHMANNJSON_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to clean external directory")
        endif()
    else()
        message(STATUS "nlohmann/json installed. Make sure it's not damaged")
    endif()

    message(STATUS "Checking for PcapPlusPlus...")
    if(NOT EXISTS ${PCAPPP_LIB_DIR})
        if(NOT EXISTS "${PCAPPP_DIR}")
            message(STATUS "PcapPlusPlus not found. Cloning from GitHub...")
            execute_process(
                    COMMAND git clone --depth 1 --branch v25.05
                    https://github.com/seladb/PcapPlusPlus.git
                    ${PCAPPP_DIR}
                    RESULT_VARIABLE GIT_CLONE_RESULT
            )
            if(NOT GIT_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone PcapPlusPlus repository")
            endif()
        endif()

        message(STATUS "PcapPlusPlus not found. Building...")
        if(N GREATER 0)
            execute_process(
                    COMMAND COMMAND make -j ${N} -f "${PCAPPP_MAKEFILE}" get_pcapplusplus
                    RESULT_VARIABLE PCAPPP_MAKE_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        else()
            execute_process(
                    COMMAND make -f "${PCAPPP_MAKEFILE}" get_pcapplusplus
                    RESULT_VARIABLE PCAPPP_MAKE_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endif()
        if(NOT PCAPPP_MAKE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build PcapPlusPlus")
        endif()

        message(STATUS "Removing PcapPlusPlus external source...")
        execute_process(
                COMMAND make -f "${PCAPPP_MAKEFILE}" remove_external
                RESULT_VARIABLE PCAPPP_REMOVE_RESULT
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT PCAPPP_REMOVE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to remove PcapPlusPlus external source")
        endif()
    else()
        message(STATUS "PcapPlusPlus directory exists, make sure it's not damaged")
    endif()

elseif(WIN32)
    message(STATUS "Checking for boost...")
    if(NOT EXISTS ${BOOST_LIB_DIR})
        message(STATUS "Boost not found. Preparing script to install to third-party...")
        execute_process(
            COMMAND cmd /c ${BOOST_BAT}
            RESULT_VARIABLE BOOST_LIB_BAT_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT BOOST_LIB_BAT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to install BOOST")
        else()
            message(STATUS "Installation completed")
        endif()
    else()
        message(STATUS "Boost installed. Make sure it's not damaged")
    endif()

    message(STATUS "Checking for nlohmann/json...")
    if(NOT EXISTS ${BOOST_LIB_DIR})
        if(NOT EXISTS "${NLOHMANNJSON_DIR}")
            message(STATUS "nlohmann/json not found. Cloning from GitHub...")
            execute_process(
                    COMMAND git clone --depth 1 --branch v3.11.3
                    https://github.com/nlohmann/json.git
                    ${NLOHMANNJSON_DIR}
                    RESULT_VARIABLE GIT_CLONE_RESULT
            )
            if(NOT GIT_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone nlohmann/json repository")
            endif()
        endif()
        message(STATUS "nlohmann/json not found. Preparing script to install to third-party...")
        execute_process(
            COMMAND cmd /c ${NLOHMANNJSON_BAT}
            RESULT_VARIABLE NLOHMANNJSON_LIB_BAT_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT NLOHMANNJSON_BAT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to install nlohmann/json")
        else()
            message(STATUS "Installation completed")
        endif()
    else()
        message(STATUS "nlohmann/json installed. Make sure it's not damaged")
    endif()

    message(STATUS "Checking for PcapPlusPlus...")
    if(NOT EXISTS ${PCAPPP_LIB_DIR})
        if(NOT EXISTS "${PCAPPP_DIR}")
            message(STATUS "PcapPlusPlus not found. Cloning from GitHub...")
            execute_process(
                    COMMAND git clone --depth 1 --branch v25.05
                    https://github.com/seladb/PcapPlusPlus.git
                    ${PCAPPP_DIR}
                    RESULT_VARIABLE GIT_CLONE_RESULT
            )
            if(NOT GIT_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone PcapPlusPlus repository")
            endif()
        endif()

        message(STATUS "PcapPlusPlus not found. Building...")
        if(N GREATER 0)
            execute_process(
                    COMMAND cmd /c "${PCAPPP_BAT}" get_pcapplusplus ${N}
                    RESULT_VARIABLE PCAPPP_BAT_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        else()
            execute_process(
                    COMMAND cmd /c "${PCAPPP_BAT}" get_pcapplusplus
                    RESULT_VARIABLE PCAPPP_BAT_RESULT
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endif()
        if(NOT PCAPPP_BAT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build PcapPlusPlus")
        endif()

        message(STATUS "Removing PcapPlusPlus external source...")
        execute_process(
                COMMAND cmd /c "${PCAPPP_BAT}" remove_external
                RESULT_VARIABLE PCAPPP_REMOVE_RESULT
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if(NOT PCAPPP_REMOVE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to remove PcapPlusPlus external source")
        endif()
    else()
        message(STATUS "PcapPlusPlus directory exists, make sure it's not damaged")
    endif()

endif()
add_subdirectory(libs/common)

if(BUILD_CLIENT)
    add_subdirectory(apps/client)
endif()

if(BUILD_SERVER)
    add_subdirectory(apps/server)
endif()
