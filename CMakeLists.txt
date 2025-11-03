cmake_minimum_required(VERSION 3.20)
project(brightpanda VERSION 1.0.0 LANGUAGES C)

# C Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# macOS specific: silence deprecation warnings for now
if(APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)

# Tree-sitter
pkg_check_modules(TREE_SITTER REQUIRED tree-sitter)

# json-c
pkg_check_modules(JSON_C REQUIRED json-c)

# Tree-sitter Python grammar
find_library(TREE_SITTER_PYTHON 
    NAMES tree-sitter-python 
    PATHS /opt/homebrew/lib /usr/local/lib
    NO_DEFAULT_PATH
)
if(NOT TREE_SITTER_PYTHON)
    message(FATAL_ERROR "tree-sitter-python not found in /opt/homebrew/lib or /usr/local/lib")
endif()

# Collect source files
set(CORE_SOURCES
    src/core/manifest.c
    src/core/entity.c
    src/core/walker.c
    src/core/parser_pool.c
    src/core/extractor.c
    src/core/cache.c
)

set(LANG_SOURCES
    src/lang/registry.c
    src/lang/python/plugin.c
)

set(UTIL_SOURCES
    src/util/logger.c
    src/util/json.c
    src/util/path.c
)

set(ALL_SOURCES
    src/main.c
    ${CORE_SOURCES}
    ${LANG_SOURCES}
    ${UTIL_SOURCES}
)

# Main executable
add_executable(brightpanda ${ALL_SOURCES})

# Set rpath for macOS (MUST come after add_executable)
if(APPLE)
    set_target_properties(brightpanda PROPERTIES
        BUILD_RPATH "/opt/homebrew/lib"
        INSTALL_RPATH "/opt/homebrew/lib"
    )
endif()

# Include directories
target_include_directories(brightpanda PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TREE_SITTER_INCLUDE_DIRS}
    ${JSON_C_INCLUDE_DIRS}
)

# Link directories
target_link_directories(brightpanda PRIVATE
    ${TREE_SITTER_LIBRARY_DIRS}
    ${JSON_C_LIBRARY_DIRS}
)

# Fix install name for tree-sitter-python on macOS
if(APPLE AND TREE_SITTER_PYTHON)
    add_custom_command(TARGET brightpanda POST_BUILD
        COMMAND install_name_tool -change libtree-sitter-python.dylib 
                ${TREE_SITTER_PYTHON} $<TARGET_FILE:brightpanda>
        COMMENT "Fixing tree-sitter-python library path"
    )
endif()

# Link libraries
target_link_libraries(brightpanda PRIVATE
    ${TREE_SITTER_LINK_LIBRARIES}
    ${JSON_C_LINK_LIBRARIES}
    ${TREE_SITTER_PYTHON}
    pthread
)

# Install targets
install(TARGETS brightpanda DESTINATION bin)

# Install query files
install(DIRECTORY src/lang/python/queries/
    DESTINATION share/brightpanda/queries/python
    FILES_MATCHING PATTERN "*.scm"
)

# Tests (optional, can be enabled with -DBUILD_TESTS=ON)
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Brightpanda Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:     ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  tree-sitter:    ${TREE_SITTER_LINK_LIBRARIES}")
message(STATUS "  json-c:         ${JSON_C_LINK_LIBRARIES}")
message(STATUS "  ts-python:      ${TREE_SITTER_PYTHON}")
message(STATUS "")