cmake_minimum_required(VERSION 3.21)
project(HSET_GeneratorClient)

set(CLIENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CLIENT_BLD "${CMAKE_BINARY_DIR}/client")

# Qt auto settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if (UNIX)
  # Attempt to find Qt first
  find_package(Qt6 COMPONENTS Core Charts Network Widgets Gui QUIET)

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
elseif(WIN32)
  # Attempt to find Qt first
  find_package(Qt6 COMPONENTS Core Charts Widgets Gui QUIET)
  if (NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please install it from official site: \nhttps://www.qt.io/download-qt-installer-oss \nand set CMAKE_PREFIX_PATH.\nIf Qt installed but you get this error, run CMake with \n-DCMAKE_PREFIX_PATH=/path/to/Qt6/lib/cmake")
  else()
    message(STATUS "Qt found")
  endif()
  message(STATUS "Using Qt version: ${Qt6_VERSION}")
endif()

find_package(Qt6 COMPONENTS LinguistTools REQUIRED)
if (NOT Qt6_LRELEASE_EXECUTABLE)
  message(FATAL_ERROR "Qt Linguist tools not found. Please install Qt Linguist tools.")
endif()

# TS source files
set(TS_FILES
  ${CLIENT_SRC}/Translations/TrafficGenerator_uk.ts
  ${CLIENT_SRC}/Translations/TrafficGenerator_en.ts
)

# Directory where QM files should go
set(TRANSLATIONS_OUTPUT_DIR ${CLIENT_BLD}/Translations)

# Ensure output directory exists
file(MAKE_DIRECTORY ${TRANSLATIONS_OUTPUT_DIR})

add_custom_target(translations ALL)

foreach(_ts_file ${TS_FILES})
  # Extract base name without extension
  get_filename_component(_ts_name ${_ts_file} NAME_WE)

  # Define output qm file path
  set(_qm_file ${TRANSLATIONS_OUTPUT_DIR}/${_ts_name}.qm)

  # Compile .ts to .qm
  add_custom_command(
    OUTPUT ${_qm_file}
    COMMAND ${Qt6_LRELEASE_EXECUTABLE} ${_ts_file} -qm ${_qm_file}
    DEPENDS ${_ts_file}
    COMMENT "Compiling translation file: ${_qm_file}"
  )

  # Add per-language target
  add_custom_target(translate_${_ts_name} ALL
    DEPENDS ${_qm_file}
  )

  # Add to main translations target
  add_dependencies(translations translate_${_ts_name})

  # Collect all .qm files
  list(APPEND QM_FILES ${_qm_file})
endforeach()

# Project sources
set(ALL_SOURCES
  ${CLIENT_SRC}/Network/NetworkManager.cpp
  ${CLIENT_SRC}/UI/HelpPage.cpp
  ${CLIENT_SRC}/UI/ConstructorPage/PacketTableModel.cpp
  ${CLIENT_SRC}/PcapUtils/PcapReader.cpp
  ${CLIENT_SRC}/PcapUtils/PacketBuilder.cpp
  ${CLIENT_SRC}/UI/ConstructorPage/ConstructorPage.cpp
  ${CLIENT_SRC}/UI/MainWindow.cpp
  ${CLIENT_SRC}/UI/ServerSelect.cpp
  ${CLIENT_SRC}/UI/GeneratorPage/RandomGenPage/ParamRow.cpp
  ${CLIENT_SRC}/UI/GeneratorPage/StatisticWindow.cpp
  ${CLIENT_SRC}/UI/GeneratorPage/RandomGenPage/GeneratorParams.cpp
  ${CLIENT_SRC}/UI/GeneratorPage/PlayerPage/GeneratorPlayer.cpp
  ${CLIENT_SRC}/UI/GeneratorPage/GeneratorPage.cpp
  ${CLIENT_SRC}/main.cpp
)

# Main target
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# Add Boost
set(BOOST_DIR "${CMAKE_SOURCE_DIR}/third-party/boost")
if(EXISTS ${BOOST_DIR})
  target_include_directories(${PROJECT_NAME} PRIVATE ${BOOST_DIR})
endif()

# Add nlohmann
set(NLOHMANN_DIR "${CMAKE_SOURCE_DIR}/third-party/nlohmann_json")
if(EXISTS ${NLOHMANN_DIR})
  target_include_directories(${PROJECT_NAME} PRIVATE ${NLOHMANN_DIR})
endif()

# Include source directory
target_include_directories(${PROJECT_NAME} PUBLIC
  ${CLIENT_BLD}
  ${CLIENT_SRC}/UI
  ${CLIENT_SRC}/UI/GeneratorPage
  ${CLIENT_SRC}/UI/GeneratorPage/RandomGenPage
  ${CLIENT_SRC}/UI/GeneratorPage/PlayerPage
  ${CLIENT_SRC}/UI/ConstructorPage
  ${CLIENT_SRC}/Network
)

# Compile options
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
  target_compile_options(${PROJECT_NAME} PRIVATE -mssse3)
endif()

# Link Qt and DPDK libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  CommonLibrary
  Qt6::Charts
  Qt6::Core
  Qt6::Gui
  Qt6::Network
  Qt6::Widgets
)

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES 
  RUNTIME_OUTPUT_DIRECTORY ${CLIENT_BLD}
)

if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE ON
  )
  string(REGEX REPLACE "/lib/cmake/?$" "/bin" QT_BASE_DIR "${Qt6_DIR}")
  set(QTWINDEPLOY "${QT_BASE_DIR}/bin/windeployqt6.exe")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${QTWINDEPLOY} 
      --no-compiler-runtime
#      --no-opengl-sw
      --dir "${CLIENT_BLD}"
      "${CLIENT_BLD}/${PROJECT_NAME}.exe"
    COMMENT "Add dlls to client"
  )
endif()
