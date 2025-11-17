# Memfault HID Library Tests

This directory contains test programs for the memfault_hid library using mocked implementations for testing without physical hardware or external dependencies.

## Test Suites

The tests are split into three independent test suites:

### 1. HID Tests (`test_hid`)
Tests HID device communication and MDS protocol functionality with mock hidapi.

**Files:**
- **test_client.c**: HID and MDS protocol tests
- **mock_hidapi.c**: Mock implementation of hidapi that simulates a HID device

**Tests covered:**
- HID device enumeration and opening
- Report communication (input/output/feature reports)
- Report filtering
- MDS session management
- MDS device configuration reading
- MDS streaming control
- MDS packet reading and sequence validation

### 2. Upload Tests (`test_upload`)
Tests chunk upload functionality with mock libcurl. This suite tests the built-in HTTP uploader.

**Files:**
- **test_upload.c**: Upload functionality tests
- **mock_libcurl.c**: Mock implementation of libcurl for HTTP testing
- **stub_hidapi.c**: Stub HID functions (not called in upload tests)

**Tests covered:**
- Custom upload callback functionality
- Uploader lifecycle management
- HTTP request success/failure handling
- Upload statistics tracking
- Error handling (network errors, HTTP errors, invalid auth)

### 3. End-to-End Integration Test (`test_mds_e2e`)
Simulates the complete MDS gateway workflow without requiring physical hardware.

**Files:**
- **test_mds_e2e.c**: Complete gateway workflow test
- **mock_hidapi.c**: Simulates HID device
- **mock_libcurl.c**: Simulates HTTP upload to cloud

**Workflow tested:**
1. Initialize library and open device
2. Create MDS session
3. Read device configuration
4. Set up uploader with mock HTTP
5. Enable streaming
6. Process stream packets
7. Upload chunks to mock cloud
8. Verify upload statistics
9. Clean shutdown

**Why this test is useful:**
- ✅ Validates entire gateway workflow without hardware
- ✅ Tests integration between HID, MDS protocol, and upload layers
- ✅ Verifies that all components work together correctly
- ✅ Can be run in CI/CD pipelines
- ✅ Provides confidence before testing with physical devices

## Mock HID Device

The mock hidapi simulates a USB HID device with the following configuration:

- **VID**: 0x1234
- **PID**: 0x5678
- **Manufacturer**: Memfault Test
- **Product**: Mock HID Device
- **Serial**: TEST-001

### Mock Behavior

- **Output Reports**: Automatically echoed back as input reports with the same Report ID (queued)
- **Feature Reports**: Stored in memory and returned when requested
- **Report IDs**: Supports any Report ID (0x01-0xFF)
- **Report Filtering**: The library's report filtering is fully testable with the mock
- **Non-blocking Mode**: Properly simulates non-blocking reads
- **MDS Protocol**: Fully supports Memfault Diagnostic Service protocol with simulated device config and stream data

### Advantages of Mocking

- ✅ No system permissions required
- ✅ Fully cross-platform (no macOS-only code)
- ✅ Deterministic testing
- ✅ No timing issues or race conditions
- ✅ Complete control over device behavior

## Usage

### Building

The test program is built automatically if `BUILD_TESTS` is enabled (default):

```bash
mkdir build
cd build
cmake ..
make
```

The test executable will be in `build/test/test_client`.

The test is compiled with the mock hidapi implementation, so it doesn't depend on the actual hidapi library for testing.

### Running Tests

#### Option 1: Run all test suites with make test

```bash
cd build
make test
```

This runs both test suites via CTest and reports pass/fail status.

#### Option 2: Run individual test suites

```bash
cd build

# Run HID tests only
./test/test_hid

# Run upload tests only
./test/test_upload

# Run end-to-end integration test
./test/test_mds_e2e
```

#### Option 3: Run with CTest for detailed output

```bash
cd build

# Run all tests with output
ctest --output-on-failure

# Run specific test suite
ctest -R HID_Tests --verbose
ctest -R Upload_Tests --verbose

# Run with extra verbosity
ctest -V
```

### Expected Output

**Using `make test`:**

```
Running tests...
Test project /path/to/build
    Start 1: HID_Tests
1/3 Test #1: HID_Tests ........................   Passed    0.11 sec
    Start 2: Upload_Tests
2/3 Test #2: Upload_Tests .....................   Passed    0.01 sec
    Start 3: MDS_E2E_Test
3/3 Test #3: MDS_E2E_Test .....................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 3

Total Test time (real) =   0.13 sec
```

**HID Test Suite (`./test/test_hid`):**

```
Memfault HID Library Test Suite
================================

=== Test 1: Library Initialization ===
[MOCK] hid_init()
  ✓ Library initialized successfully

...

========================================
Test Summary
========================================
Total tests:  20
Assertions:   51 total (51 passed, 0 failed)
Result:       PASS
========================================
```

**Upload Test Suite (`./test/test_upload`):**

```
MDS Upload Test Suite
=====================

=== Test 1: Custom Upload Callback ===
  ✓ Callback returns success
  ✓ Upload count incremented
  ✓ URI captured correctly
  ✓ Auth captured correctly
  ✓ Chunk length correct

...

========================================
Test Summary
========================================
Total tests:  12
Assertions:   40 total (40 passed, 0 failed)
Result:       PASS
========================================
```

**End-to-End Integration Test (`./test/test_mds_e2e`):**

```
╔════════════════════════════════════════════════════════════╗
║  MDS Gateway End-to-End Integration Test                  ║
║  Tests complete workflow with mocked device and cloud     ║
╚════════════════════════════════════════════════════════════╝

▸ Initializing HID library
  ✓ Library initialized

▸ Opening mock HID device
  ✓ Device opened
  ✓ Device handle is valid

▸ Creating MDS session
  ✓ MDS session created
  ✓ Session handle is valid

▸ Reading device configuration
  ✓ Configuration read successfully
  Device ID:     test-device-12345
  Data URI:      https://chunks.memfault.com/api/v0/chunks/test-device
  Authorization: Memfault-Project-Key:test_project_key_12345
  Features:      0x00000000

▸ Processing stream packets
  Processing up to 5 chunks...
  Chunk 1 processed
    Uploaded: 1 chunks, 19 bytes, status: 202
  Chunk 2 processed
    Uploaded: 2 chunks, 38 bytes, status: 202
  Chunk 3 processed
    Uploaded: 3 chunks, 57 bytes, status: 202

▸ Verifying upload statistics
  Chunks uploaded:   3
  Bytes uploaded:    57
  Upload failures:   0
  Last HTTP status:  202
  ✓ All assertions passed

╔════════════════════════════════════════════════════════════╗
║  Test Summary                                              ║
╠════════════════════════════════════════════════════════════╣
║  Assertions Passed:  23                                    ║
║  Assertions Failed:  0                                     ║
╠════════════════════════════════════════════════════════════╣
║  Result: ✓ ALL TESTS PASSED                               ║
╚════════════════════════════════════════════════════════════╝
```

**Test Coverage:**
- **HID Tests (20 tests, 51 assertions)**: Core HID functionality, MDS protocol, session management, streaming
- **Upload Tests (12 tests, 40 assertions)**: HTTP upload functionality, error handling, statistics
- **E2E Integration Test (23 assertions)**: Complete gateway workflow from device to cloud

The `[MOCK]` prefix shows which hidapi functions are being called, helping with debugging and understanding the test flow.

## Manual Testing with Real Devices

You can also test the library with real HID devices using the example programs:

```bash
# Enumerate all HID devices
cd build/examples
./enumerate_devices

# Enumerate specific VID/PID
./enumerate_devices 046d c52b

# Test send/receive with a real device
./send_receive 046d c52b
```

## MDS Protocol Testing

The mock implementation includes full support for the Memfault Diagnostic Service (MDS) protocol:

### Simulated MDS Device Configuration

When a device is opened, the mock automatically initializes MDS feature reports:

- **Supported Features** (Report 0x01): `0x00000000` (version 1)
- **Device Identifier** (Report 0x02): `"test-device-12345"`
- **Data URI** (Report 0x03): `"https://chunks.memfault.com/api/v0/chunks/test-device"`
- **Authorization** (Report 0x04): `"Memfault-Project-Key:test_project_key_12345"`

### Stream Behavior

When streaming is enabled (Report 0x05 with value 0x01):
- Mock queues 3 diagnostic chunk packets automatically
- Each packet has proper MDS format (sequence byte + payload)
- Sequence numbers increment properly (0, 1, 2)
- Packets are queued as input reports with Report ID 0x06

### Test Coverage

MDS tests verify:
- ✅ Session creation and destruction
- ✅ Reading device configuration (all fields)
- ✅ Individual config item reads (features, device ID, URI, auth)
- ✅ Stream enable/disable commands
- ✅ Packet reading with proper sequence extraction
- ✅ Sequence validation (wrapping, dropped packets, duplicates)

## Mock Implementations

### Mock HIDAPI (`mock_hidapi.c`)

The mock hidapi implementation provides:

### Simulated Device
- Returns a single mock device on enumeration (VID:0x1234, PID:0x5678)
- Path: "mock://device/1"
- Manufacturer: "Memfault Test"
- Product: "Mock HID Device"

### Report Handling
- **Output Reports**: Queued as input reports (echo behavior)
- **Input Reports**: Returns queued reports, or 0 for timeout
- **Feature Reports**: Stored by Report ID in memory
- **Report Queue**: Up to 10 reports can be queued

### Debug Output
All mock functions print debug messages prefixed with `[MOCK]`, showing:
- Function calls and parameters
- Report IDs and data (first 16 bytes)
- Queue status
- Feature report storage

This helps understand the exact sequence of HID operations during testing.

### Mock libcurl (`mock_libcurl.c`)

The mock libcurl implementation provides:

**HTTP Request Simulation:**
- Tracks URL, headers, and POST data for each request
- Configurable HTTP response codes (200, 404, 500, etc.)
- Configurable libcurl error codes (CURLE_OK, CURLE_COULDNT_CONNECT, etc.)
- Request counting and statistics

**Control Functions:**
- `mock_curl_reset()` - Reset state to defaults
- `mock_curl_set_response(http_code, error)` - Configure next request result
- `mock_curl_get_request_count()` - Get number of requests made
- `mock_curl_get_last_url()` - Inspect last URL requested

**Debug Output:**
All mock functions print debug messages prefixed with `[MOCK CURL]`, showing:
- curl_easy_* function calls and parameters
- HTTP requests and responses
- URL and header information

This enables testing upload functionality without actual network requests.

## Extending Tests

### Adding HID/MDS Protocol Tests

Add test cases in `test_client.c`:
```c
TEST_START("Test Name");
// ... test code ...
TEST_ASSERT(condition, "Assertion description");
```

Modify mock behavior in `mock_hidapi.c`:
- Simulate different device responses
- Add error conditions
- Test edge cases
- Modify report handling logic

### Adding Upload Tests

Add test cases in `test_upload.c`:
```c
TEST_START("Test Name");
// ... test code ...
TEST_ASSERT(condition, "Assertion description");
```

Configure mock HTTP behavior:
```c
mock_curl_reset();
mock_curl_set_response(404, CURLE_OK);  /* Simulate HTTP 404 */
/* ... run test ... */
```

### Adding New Test Suites

To add a new test suite:

1. Create new test file in `test/` directory
2. Add executable in `test/CMakeLists.txt`:
   ```cmake
   add_executable(test_new test_new.c)
   target_link_libraries(test_new ...)
   add_test(NAME New_Tests COMMAND test_new)
   ```
3. Run with `make test` or `ctest`
