# MDS (Memfault Diagnostic Service) Examples

# C Examples
# MDS gateway - uploads diagnostic chunks to Memfault cloud
add_executable(mds_gateway mds_gateway.c)
target_link_libraries(mds_gateway PRIVATE memfault_hid)

# MDS stream monitor - displays diagnostic stream data
add_executable(mds_monitor mds_monitor.c)
target_link_libraries(mds_monitor PRIVATE memfault_hid)

# Install C examples (optional)
install(TARGETS mds_gateway mds_monitor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/memfault_hid_examples
)

# Python Example - Copy to build directory with correct library paths
set(PYTHON_EXAMPLE_DIR ${CMAKE_BINARY_DIR}/examples/python)
file(MAKE_DIRECTORY ${PYTHON_EXAMPLE_DIR})

# Copy Python files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/main.py
    ${PYTHON_EXAMPLE_DIR}/main.py
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/mds_client.py
    ${PYTHON_EXAMPLE_DIR}/mds_client.py
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/hid_backend.py
    ${PYTHON_EXAMPLE_DIR}/hid_backend.py
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/requirements.txt
    ${PYTHON_EXAMPLE_DIR}/requirements.txt
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/test_bindings.py
    ${PYTHON_EXAMPLE_DIR}/test_bindings.py
    COPYONLY
)

# For Python, just copy bindings.py - it already has logic to find the library
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/bindings.py
    ${PYTHON_EXAMPLE_DIR}/bindings.py
    COPYONLY
)

# Node.js Example - Copy to build directory
set(NODEJS_EXAMPLE_DIR ${CMAKE_BINARY_DIR}/examples/nodejs)
file(MAKE_DIRECTORY ${NODEJS_EXAMPLE_DIR})

# Copy Node.js files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nodejs/index.js
    ${NODEJS_EXAMPLE_DIR}/index.js
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nodejs/mds-client.js
    ${NODEJS_EXAMPLE_DIR}/mds-client.js
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nodejs/node-hid-backend.js
    ${NODEJS_EXAMPLE_DIR}/node-hid-backend.js
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nodejs/package.json
    ${NODEJS_EXAMPLE_DIR}/package.json
    COPYONLY
)

# For Node.js, just copy bindings.js - it already has logic to find the library
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nodejs/bindings.js
    ${NODEJS_EXAMPLE_DIR}/bindings.js
    COPYONLY
)

# Print helpful message
add_custom_target(copy_examples ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Examples copied to build directory:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Python:  ${PYTHON_EXAMPLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Node.js: ${NODEJS_EXAMPLE_DIR}"
    DEPENDS memfault_hid
)
