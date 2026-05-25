project(SilLib VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sil_lib_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/sil_lib_version.h
)

add_library(sil_lib
    drivers/can_frame.c
    drivers/can_driver.c
    drivers/io_driver.c
    udp_socket/udp_socket.c
    udp_socket/sil_config_udp_socket.c
    io/io_transport_udp/io_transport_udp.c
    io/sil_io_config.c
    vcan/can_transport_udp/can_transport_udp.c
    vcan/can_emulator/can_emulator.c
    vcan/sil_vcan_config.c
)
target_include_directories(sil_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/drivers>
        $<BUILD_INTERFACE:${SIL_IO_CONFIG_ROOT}>
        $<BUILD_INTERFACE:${SIL_VCAN_CONFIG_ROOT}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/sil_lib>
    PRIVATE
        ${UDP_SOCKET_ROOT}
        ${IO_TRANSPORT_UDP_ROOT}
        ${CAN_TRANSPORT_UDP_ROOT}
        ${SIL_VCAN_CONFIG_ROOT}/can_emulator
)
target_link_libraries(sil_lib
    PUBLIC
        ${UDP_SOCKET_PLATFORM_LIBS}
)
set_module_output_layout(sil_lib "sil_lib")

# --- Install / Export ---------------------------------------------------
include(CMakePackageConfigHelpers)

install(TARGETS sil_lib
    EXPORT SilLibTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    drivers/can_frame.h
    drivers/can_driver.h
    drivers/io_driver.h
    io/sil_io_config.h
    vcan/sil_vcan_config.h
    ${CMAKE_CURRENT_BINARY_DIR}/sil_lib_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sil_lib
)

install(EXPORT SilLibTargets
    FILE SilLibTargets.cmake
    NAMESPACE SilLib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SilLib
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SilLibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SilLibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SilLib
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SilLibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SilLibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SilLibConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SilLib
)
