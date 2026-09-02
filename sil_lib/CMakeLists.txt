project(SilLib VERSION 1.0.0 LANGUAGES C)

set(SIL_LIB_VERSION ${PROJECT_VERSION} PARENT_SCOPE)

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

# WCAN is a Windows-only virtual CAN bus built on a shared-memory segment. It
# has no POSIX equivalent, so it is compiled in only on Windows.
if(WIN32)
    target_sources(sil_lib PRIVATE
        wcan/wcan_validate.c
        wcan/wcan_airtime.c
        wcan/wcan_shm.c
    )
    target_link_libraries(sil_lib PUBLIC advapi32 winmm)

    # Integration and diagnostic programs. These drive real shared memory,
    # real processes and real time, so they are deliberately not Ceedling unit
    # tests; they live in wcan/check and are built on request.
    option(WCAN_BUILD_CHECKS "Build the WCAN integration and diagnostic programs" OFF)
    if(WCAN_BUILD_CHECKS)
        foreach(wcan_program
                check/check_wcan_transport
                check/check_wcan_bittime
                check/check_wcan_layout
                tools/wcan_peer
                tools/wcan_resources)
            get_filename_component(wcan_target ${wcan_program} NAME)
            add_executable(${wcan_target} wcan/${wcan_program}.c)
            target_link_libraries(${wcan_target} PRIVATE sil_lib)
            target_include_directories(${wcan_target} PRIVATE ${SIL_WCAN_ROOT})
            set_module_output_layout(${wcan_target} "sil_lib/wcan")
        endforeach()
        target_link_libraries(wcan_resources PRIVATE psapi)
    endif()
endif()

target_include_directories(sil_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/drivers>
        $<BUILD_INTERFACE:${SIL_IO_CONFIG_ROOT}>
        $<BUILD_INTERFACE:${SIL_VCAN_CONFIG_ROOT}>
        $<BUILD_INTERFACE:${SIL_WCAN_ROOT}>
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

if(WIN32)
    # wcan_layout.h is public because it defines the cross-process ABI that
    # every node maps, including consumers built by another toolchain.
    install(FILES
        wcan/wcan.h
        wcan/wcan_types.h
        wcan/wcan_shm.h
        wcan/wcan_export.h
        wcan/wcan_layout.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sil_lib
    )
endif()

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

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/../docs/INTEGRATION.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/SDD_sil_lib.md
    ${CMAKE_CURRENT_SOURCE_DIR}/io/SDD_sil_io.md
    ${CMAKE_CURRENT_SOURCE_DIR}/vcan/SDD_sil_vcan.md
    ${CMAKE_CURRENT_SOURCE_DIR}/udp_socket/SDD_sil_udp_socket.md
    ${CMAKE_CURRENT_SOURCE_DIR}/vcan/can_emulator/SDD_sil_can_emulator.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/design
)
