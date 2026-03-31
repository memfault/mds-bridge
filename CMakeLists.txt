cmake_minimum_required(VERSION 3.15)
project(mds_bridge VERSION 2.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Export compile commands for LSP/IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
# Default to static libraries on Windows (DLLs require export declarations)
if(WIN32)
    option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
else()
    option(BUILD_SHARED_LIBS "Build shared libraries" ON)
endif()
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs (macOS only)" ON)
option(ENABLE_NODEJS "Build Node.js example and addon" ON)

# Add CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Platform detection for platform-specific libraries
if(APPLE)
    set(PLATFORM_MACOS TRUE)
endif()

# Find dependencies
find_package(hidapi REQUIRED)
find_package(CURL REQUIRED)

# Source files
set(MDS_BRIDGE_SOURCES
    src/memfault_hid.c
    src/mds_protocol.c
    src/mds_backend_hid.c
    src/chunks_uploader.c
)

# Create library target
add_library(mds_bridge ${MDS_BRIDGE_SOURCES})

# Set library properties
set_target_properties(mds_bridge PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    PUBLIC_HEADER "include/mds_bridge/mds_protocol.h;include/mds_bridge/mds_backend.h;include/mds_bridge/chunks_uploader.h;include/mds_bridge/memfault_hid.h;include/mds_bridge/platform_compat.h"
)

# Include directories
target_include_directories(mds_bridge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(mds_bridge PRIVATE hidapi::hidapi CURL::libcurl)

# Platform-specific libraries
if(PLATFORM_MACOS)
    target_link_libraries(mds_bridge PRIVATE "-framework IOKit" "-framework CoreFoundation")
elseif(PLATFORM_WINDOWS)
    target_link_libraries(mds_bridge PRIVATE setupapi)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(mds_bridge PRIVATE /W4)
else()
    target_compile_options(mds_bridge PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS mds_bridge
    EXPORT mds_bridge-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mds_bridge
)

install(DIRECTORY include/mds_bridge
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT mds_bridge-targets
    FILE mds_bridge-targets.cmake
    NAMESPACE memfault::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mds_bridge
)

# Create package config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mds_bridge-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mds_bridge-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mds_bridge
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mds_bridge-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mds_bridge-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mds_bridge-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mds_bridge
)

# AppImage target (Linux only)
if(UNIX AND NOT APPLE)
    add_custom_target(appimage
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/appimage/build-appimage.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building AppImage package..."
        VERBATIM
    )
endif()
