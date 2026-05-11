project(CommonLibrary)

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(COMMON_HEADERS
        ${COMMON_DIR}/include/generator_values.hpp
)

set(COMMON_SOURCES
        # add .cpp files here when needed
)

if(COMMON_SOURCES)
    add_library(${PROJECT_NAME} ${COMMON_SOURCES})
    target_include_directories(${PROJECT_NAME}
            PUBLIC ${COMMON_DIR}/include
    )
else()
    add_library(${PROJECT_NAME} INTERFACE)
    target_include_directories(${PROJECT_NAME}
            INTERFACE ${COMMON_DIR}/include
    )
endif()