cmake_minimum_required(VERSION 3.16)

project(HelloWorldC VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BUILD_PRODUCTION "Build production application targets" ON)
set(ARTIFACTS_ROOT "${CMAKE_SOURCE_DIR}/build/out" CACHE PATH "Output directory for final build artifacts")

set(SW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/sw)
set(UDP_SOCKET_ROOT ${SW_ROOT}/drivers/comms/udp_socket)
set(CAN_FRAME_ROOT ${SW_ROOT}/drivers/comms/can_driver/can_frame)
set(CAN_EMULATOR_ROOT ${SW_ROOT}/drivers/comms/can_driver/can_emulator)
set(CAN_DRIVER_ROOT ${SW_ROOT}/drivers/comms/can_driver)
set(CAN_TRANSPORT_UDP_ROOT ${SW_ROOT}/drivers/comms/can_driver/can_transport_udp)
set(IO_ROOT ${SW_ROOT}/drivers/io)
set(IO_TRANSPORT_UDP_ROOT ${SW_ROOT}/drivers/io/io_transport_udp)

set(UDP_SOCKET_PLATFORM_LIBS)
if(WIN32)
    list(APPEND UDP_SOCKET_PLATFORM_LIBS ws2_32)
endif()

function(set_module_output_layout target module_rel_path)
    set(base_dir "${ARTIFACTS_ROOT}/sw/${module_rel_path}")

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${base_dir}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${base_dir}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${base_dir}/lib"
    )
endfunction()

if(BUILD_PRODUCTION)
    add_library(udp_socket_lib
        sw/drivers/comms/udp_socket/src/udp_socket.c
    )

    target_include_directories(udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_ROOT}/inc
    )
    target_link_libraries(udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_PLATFORM_LIBS}
    )
    set_module_output_layout(udp_socket_lib "drivers/comms/udp_socket")

    add_library(can_frame_lib
        sw/drivers/comms/can_driver/can_frame/src/can_frame.c
    )

    target_include_directories(can_frame_lib
        PUBLIC
            ${CAN_FRAME_ROOT}/inc
    )
    set_module_output_layout(can_frame_lib "drivers/comms/can_frame")

    add_library(can_emulator_lib
        sw/drivers/comms/can_driver/can_emulator/src/can_emulator.c
    )

    target_include_directories(can_emulator_lib
        PUBLIC
            ${CAN_FRAME_ROOT}/inc
            ${CAN_EMULATOR_ROOT}/inc
    )
    target_link_libraries(can_emulator_lib
        PUBLIC
            can_frame_lib
    )
    set_module_output_layout(can_emulator_lib "drivers/comms/can_emulator")

    add_library(can_driver_lib
        sw/drivers/comms/can_driver/src/can_driver.c
    )

    target_include_directories(can_driver_lib
        PUBLIC
            ${CAN_FRAME_ROOT}/inc
            ${CAN_DRIVER_ROOT}/inc
            ${CAN_EMULATOR_ROOT}/inc
    )
    target_link_libraries(can_driver_lib
        PUBLIC
            can_emulator_lib
    )
    set_module_output_layout(can_driver_lib "drivers/comms/can_driver")

    add_library(can_transport_udp_lib
        sw/drivers/comms/can_driver/can_transport_udp/src/can_transport_udp.c
    )

    target_include_directories(can_transport_udp_lib
        PUBLIC
            ${UDP_SOCKET_ROOT}/inc
            ${CAN_FRAME_ROOT}/inc
            ${CAN_TRANSPORT_UDP_ROOT}/inc
    )
    target_link_libraries(can_transport_udp_lib
        PUBLIC
            udp_socket_lib
            can_frame_lib
    )
    set_module_output_layout(can_transport_udp_lib "drivers/comms/can_transport_udp")

    add_library(io_lib
        sw/drivers/io/src/io.c
    )

    target_include_directories(io_lib
        PUBLIC
            ${IO_ROOT}/inc
    )
    set_module_output_layout(io_lib "drivers/io")

    add_library(io_transport_udp_lib
        sw/drivers/io/io_transport_udp/src/io_transport_udp.c
    )

    target_include_directories(io_transport_udp_lib
        PUBLIC
            ${UDP_SOCKET_ROOT}/inc
            ${IO_TRANSPORT_UDP_ROOT}/inc
    )
    target_link_libraries(io_transport_udp_lib
        PUBLIC
            udp_socket_lib
    )
    set_module_output_layout(io_transport_udp_lib "drivers/io/io_transport_udp")

    add_executable(hello_world
        sw/apps/main.c
    )
    set_module_output_layout(hello_world "apps")
endif()