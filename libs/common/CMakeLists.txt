project(CommonLibrary)

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(COMMON_HEADERS
    ${COMMON_DIR}/include/generator_values.hpp
)

set(COMMON_SOURCES
)

add_library(${PROJECT_NAME}
    ${COMMON_HEADERS}
    ${COMMON_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC ${COMMON_DIR}/include
)
