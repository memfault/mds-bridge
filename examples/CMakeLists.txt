# MDS (Memfault Diagnostic Service) Examples

# C Examples
# MDS gateway - uploads diagnostic chunks to Memfault cloud
add_executable(mds_gateway mds_gateway.c)
target_link_libraries(mds_gateway PRIVATE mds_bridge)

# MDS gateway demo - CES booth version with auto-reconnect
add_executable(mds_gateway_demo mds_gateway_demo.c)
target_link_libraries(mds_gateway_demo PRIVATE mds_bridge)

# MDS stream monitor - displays diagnostic stream data
add_executable(mds_monitor mds_monitor.c)
target_link_libraries(mds_monitor PRIVATE mds_bridge)

# Install C examples (optional)
install(TARGETS mds_gateway mds_gateway_demo mds_monitor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/mds_bridge_examples
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

# For Python, just copy bindings.py - it already has logic to find the library
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/python/bindings.py
    ${PYTHON_EXAMPLE_DIR}/bindings.py
    COPYONLY
)

# Node.js Example - Build with N-API addon
if(ENABLE_NODEJS)
    set(NODEJS_EXAMPLE_DIR ${CMAKE_BINARY_DIR}/examples/nodejs)
    add_subdirectory(nodejs)
endif()

# Print helpful message
add_custom_target(copy_examples ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Examples copied to build directory:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Python:  ${PYTHON_EXAMPLE_DIR}"
    DEPENDS mds_bridge
)

if(ENABLE_NODEJS)
    add_custom_command(
        TARGET copy_examples
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "  Node.js: ${NODEJS_EXAMPLE_DIR}"
    )
endif()
