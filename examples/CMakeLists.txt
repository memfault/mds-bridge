# Examples for memfault_hid library

# Basic device enumeration example
add_executable(enumerate_devices enumerate_devices.c)
target_link_libraries(enumerate_devices PRIVATE memfault_hid)

# Send/receive reports example
add_executable(send_receive send_receive.c)
target_link_libraries(send_receive PRIVATE memfault_hid)

# Continuous communication example
add_executable(continuous_comm continuous_comm.c)
target_link_libraries(continuous_comm PRIVATE memfault_hid)

# MDS gateway example
add_executable(mds_gateway mds_gateway.c)
target_link_libraries(mds_gateway PRIVATE memfault_hid)

# Install examples (optional)
install(TARGETS enumerate_devices send_receive continuous_comm mds_gateway
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/memfault_hid_examples
)
