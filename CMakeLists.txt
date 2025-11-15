cmake_minimum_required(VERSION 3.15)
project(memfault_hid VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Export compile commands for LSP/IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs (macOS only)" ON)

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
set(MEMFAULT_HID_SOURCES
    src/memfault_hid.c
    src/memfault_hid_transport.c
    src/mds_protocol.c
    src/mds_upload.c
)

# Create library target
add_library(memfault_hid ${MEMFAULT_HID_SOURCES})

# Set library properties
set_target_properties(memfault_hid PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/memfault_hid/memfault_hid.h
)

# Include directories
target_include_directories(memfault_hid
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(memfault_hid PRIVATE hidapi::hidapi CURL::libcurl)

# Platform-specific libraries
if(PLATFORM_MACOS)
    target_link_libraries(memfault_hid PRIVATE "-framework IOKit" "-framework CoreFoundation")
elseif(PLATFORM_WINDOWS)
    target_link_libraries(memfault_hid PRIVATE setupapi)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(memfault_hid PRIVATE /W4)
else()
    target_compile_options(memfault_hid PRIVATE -Wall -Wextra -Wpedantic)
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

install(TARGETS memfault_hid
    EXPORT memfault_hid-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/memfault_hid
)

install(DIRECTORY include/memfault_hid
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT memfault_hid-targets
    FILE memfault_hid-targets.cmake
    NAMESPACE memfault::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memfault_hid
)

# Create package config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/memfault_hid-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/memfault_hid-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memfault_hid
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/memfault_hid-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/memfault_hid-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/memfault_hid-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memfault_hid
)
