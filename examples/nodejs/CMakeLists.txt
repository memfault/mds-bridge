# Node.js example CMake integration
# This copies the Node.js example to the build directory and optionally builds the native addon

# Copy all necessary files to build directory
configure_file(package.json ${CMAKE_CURRENT_BINARY_DIR}/package.json COPYONLY)
configure_file(package-lock.json ${CMAKE_CURRENT_BINARY_DIR}/package-lock.json COPYONLY)
configure_file(index.js ${CMAKE_CURRENT_BINARY_DIR}/index.js COPYONLY)
configure_file(mds-native.js ${CMAKE_CURRENT_BINARY_DIR}/mds-native.js COPYONLY)
configure_file(README.md ${CMAKE_CURRENT_BINARY_DIR}/README.md COPYONLY)

# Configure binding.gyp with correct absolute paths
configure_file(binding.gyp.in ${CMAKE_CURRENT_BINARY_DIR}/binding.gyp @ONLY)

# Copy src directory
file(COPY src/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/src)

# Add custom target for npm install and build
add_custom_target(nodejs_example ALL
    COMMENT "Building Node.js example with N-API addon"
)

# Check if npm is available
find_program(NPM_EXECUTABLE npm)

if(NPM_EXECUTABLE)
    message(STATUS "Found npm: ${NPM_EXECUTABLE}")

    # Add custom command to run npm install (which builds the addon)
    add_custom_command(
        TARGET nodejs_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Installing Node.js dependencies and building N-API addon..."
        COMMAND ${NPM_EXECUTABLE} install --loglevel=error
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running npm install in ${CMAKE_CURRENT_BINARY_DIR}"
        VERBATIM
    )

    # Create a helpful message
    add_custom_command(
        TARGET nodejs_example
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
        COMMAND ${CMAKE_COMMAND} -E echo "Node.js example built successfully!"
        COMMAND ${CMAKE_COMMAND} -E echo "Location: ${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "To run the example:"
        COMMAND ${CMAKE_COMMAND} -E echo "  cd ${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "  npm start -- <VID> <PID>"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "Example:"
        COMMAND ${CMAKE_COMMAND} -E echo "  npm start -- 2fe3 7"
        COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
        COMMAND ${CMAKE_COMMAND} -E echo ""
        VERBATIM
    )
else()
    message(WARNING "npm not found - Node.js example will be copied but not built")
    message(WARNING "Install Node.js and npm, then run 'npm install' in ${CMAKE_CURRENT_BINARY_DIR}")
endif()

# Make the Node.js example depend on the main library
add_dependencies(nodejs_example memfault_hid)
