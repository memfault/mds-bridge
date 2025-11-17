# Test programs for memfault_hid library

# Enable CTest for "make test" support
enable_testing()

# ============================================================================
# Test Suite 1: HID and MDS Protocol Tests (mocks hidapi)
# ============================================================================

# Build HID test client with mocked hidapi
# We compile the library sources directly with the mock to avoid linking issues

add_executable(test_hid
    test_client.c
    mock_hidapi.c
    ${CMAKE_SOURCE_DIR}/src/memfault_hid.c
    ${CMAKE_SOURCE_DIR}/src/memfault_hid_transport.c
    ${CMAKE_SOURCE_DIR}/src/mds_protocol.c
)

# Include directories for HID tests
target_include_directories(test_hid PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${HIDAPI_INCLUDE_DIR}
)

# The mock provides hidapi symbols, so no need to link real hidapi
# But we still need platform-specific libraries if on macOS
if(APPLE)
    target_link_libraries(test_hid PRIVATE
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

# Add to CTest
add_test(NAME HID_Tests COMMAND test_hid)

# ============================================================================
# Test Suite 2: Upload Tests (mocks libcurl)
# ============================================================================

add_executable(test_upload
    test_upload.c
    mock_libcurl.c
    stub_hidapi.c
    ${CMAKE_SOURCE_DIR}/src/mds_upload.c
    ${CMAKE_SOURCE_DIR}/src/mds_protocol.c
)

# Include directories for upload tests
target_include_directories(test_upload PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${CURL_INCLUDE_DIRS}
)

# Add to CTest
add_test(NAME Upload_Tests COMMAND test_upload)

# ============================================================================
# Test Suite 3: End-to-End Integration Test (mocks both hidapi and libcurl)
# ============================================================================

add_executable(test_mds_e2e
    test_mds_e2e.c
    mock_hidapi.c
    mock_libcurl.c
    ${CMAKE_SOURCE_DIR}/src/memfault_hid.c
    ${CMAKE_SOURCE_DIR}/src/memfault_hid_transport.c
    ${CMAKE_SOURCE_DIR}/src/mds_protocol.c
    ${CMAKE_SOURCE_DIR}/src/mds_upload.c
)

# Include directories for e2e test
target_include_directories(test_mds_e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${HIDAPI_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# Platform-specific libraries for macOS
if(APPLE)
    target_link_libraries(test_mds_e2e PRIVATE
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

# Add to CTest
add_test(NAME MDS_E2E_Test COMMAND test_mds_e2e)

# Installation (optional)
install(TARGETS test_hid test_upload test_mds_e2e
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/memfault_hid_tests
)

message(STATUS "HID tests will be built with mock hidapi")
message(STATUS "Upload tests will be built with mock libcurl")
message(STATUS "E2E tests will be built with both mocks for full integration testing")
message(STATUS "Run 'make test' or 'ctest' to execute all test suites")
