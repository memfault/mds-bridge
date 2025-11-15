# FindHIDAPI.cmake - Find HIDAPI library
#
# This module defines:
#  hidapi_FOUND - System has HIDAPI
#  hidapi::hidapi - Imported target for HIDAPI

find_path(HIDAPI_INCLUDE_DIR
    NAMES hidapi.h
    PATH_SUFFIXES hidapi
)

if(WIN32)
    find_library(HIDAPI_LIBRARY
        NAMES hidapi
    )
elseif(APPLE)
    find_library(HIDAPI_LIBRARY
        NAMES hidapi
    )
else()
    # Linux - try both hidapi-libusb and hidapi-hidraw
    find_library(HIDAPI_LIBRARY
        NAMES hidapi-libusb hidapi-hidraw hidapi
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(hidapi
    REQUIRED_VARS HIDAPI_LIBRARY HIDAPI_INCLUDE_DIR
)

if(hidapi_FOUND AND NOT TARGET hidapi::hidapi)
    add_library(hidapi::hidapi UNKNOWN IMPORTED)
    set_target_properties(hidapi::hidapi PROPERTIES
        IMPORTED_LOCATION "${HIDAPI_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${HIDAPI_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(HIDAPI_INCLUDE_DIR HIDAPI_LIBRARY)
