cmake_minimum_required(VERSION 3.16)

project(SIL VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BUILD_PRODUCTION "Build production application targets" ON)
set(ARTIFACTS_ROOT "${CMAKE_SOURCE_DIR}/build/out" CACHE PATH "Output directory for final build artifacts")

set(SW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/sw)
set(SIL_LIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/sil_lib)
set(CAN_FRAME_ROOT ${SW_ROOT}/drivers/can_driver/can_frame)
set(CAN_DRIVER_ROOT ${SW_ROOT}/drivers/can_driver)
set(UDP_SOCKET_ROOT ${SIL_LIB_ROOT}/udp_socket)
set(CAN_TRANSPORT_UDP_ROOT ${SIL_LIB_ROOT}/vcan/can_transport_udp)
set(IO_TRANSPORT_UDP_ROOT ${SIL_LIB_ROOT}/io/io_transport_udp)
set(PI_CONTROLLER_ROOT ${SW_ROOT}/modules/pi_controller)
set(FAN_CONTROLLER_ROOT ${SW_ROOT}/modules/fan_controller)
set(IO_DRIVER_ROOT ${SW_ROOT}/drivers/io)
set(SIL_IO_CONFIG_ROOT ${SIL_LIB_ROOT}/io)
set(SIL_VCAN_CONFIG_ROOT ${SIL_LIB_ROOT}/vcan)

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
        sil_lib/udp_socket/udp_socket.c
    )
    target_include_directories(udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_ROOT}
    )
    target_link_libraries(udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_PLATFORM_LIBS}
    )
    set_module_output_layout(udp_socket_lib "sil_lib/udp_socket")

    add_library(can_frame_lib
        sw/drivers/can_driver/can_frame/can_frame.c
    )
    target_include_directories(can_frame_lib
        PUBLIC
            ${CAN_FRAME_ROOT}
    )
    set_module_output_layout(can_frame_lib "drivers/can_driver/can_frame")

    add_library(can_transport_udp_lib
        sil_lib/vcan/can_transport_udp/can_transport_udp.c
    )
    target_include_directories(can_transport_udp_lib
        PUBLIC
            ${CAN_TRANSPORT_UDP_ROOT}
            ${CAN_FRAME_ROOT}
            ${UDP_SOCKET_ROOT}
    )
    target_link_libraries(can_transport_udp_lib
        PUBLIC
            udp_socket_lib
            can_frame_lib
    )
    set_module_output_layout(can_transport_udp_lib "sil_lib/vcan/can_transport_udp")

    add_library(can_driver_lib
        sw/drivers/can_driver/can_driver.c
    )
    target_include_directories(can_driver_lib
        PUBLIC
            ${CAN_DRIVER_ROOT}
    )
    target_link_libraries(can_driver_lib
        PUBLIC
            can_frame_lib
    )
    set_module_output_layout(can_driver_lib "drivers/can_driver")

    add_library(io_transport_udp_lib
        sil_lib/io/io_transport_udp/io_transport_udp.c
    )
    target_include_directories(io_transport_udp_lib
        PUBLIC
            ${IO_TRANSPORT_UDP_ROOT}
            ${UDP_SOCKET_ROOT}
    )
    target_link_libraries(io_transport_udp_lib
        PUBLIC
            udp_socket_lib
    )
    set_module_output_layout(io_transport_udp_lib "sil_lib/io/io_transport_udp")

    add_library(io_driver_lib
        sw/drivers/io/io_driver.c
    )
    target_include_directories(io_driver_lib
        PUBLIC
            ${IO_DRIVER_ROOT}
    )
    set_module_output_layout(io_driver_lib "drivers/io")

    add_library(sil_config_udp_socket_lib
        sil_lib/udp_socket/sil_config_udp_socket.c
    )
    target_include_directories(sil_config_udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_ROOT}
    )
    target_link_libraries(sil_config_udp_socket_lib
        PUBLIC
            udp_socket_lib
    )
    set_module_output_layout(sil_config_udp_socket_lib "sil_lib/udp_socket")

    add_library(sil_io_config_lib
        sil_lib/io/sil_io_config.c
    )
    target_include_directories(sil_io_config_lib
        PUBLIC
            ${SIL_IO_CONFIG_ROOT}
    )
    target_link_libraries(sil_io_config_lib
        PUBLIC
            io_driver_lib
        PRIVATE
            io_transport_udp_lib
            sil_config_udp_socket_lib
    )
    set_module_output_layout(sil_io_config_lib "sil_lib/io")

    add_library(sil_vcan_config_lib
        sil_lib/vcan/sil_vcan_config.c
    )
    target_include_directories(sil_vcan_config_lib
        PUBLIC
            ${SIL_VCAN_CONFIG_ROOT}
    )
    target_link_libraries(sil_vcan_config_lib
        PUBLIC
            can_driver_lib
        PRIVATE
            can_transport_udp_lib
            sil_config_udp_socket_lib
    )
    set_module_output_layout(sil_vcan_config_lib "sil_lib/vcan")

    add_library(pi_controller_lib
        sw/modules/pi_controller/pi_controller.c
    )
    target_include_directories(pi_controller_lib
        PUBLIC
            ${PI_CONTROLLER_ROOT}
    )
    set_module_output_layout(pi_controller_lib "modules/pi_controller")

    add_library(fan_controller_lib
        sw/modules/fan_controller/fan_controller.c
    )
    target_include_directories(fan_controller_lib
        PUBLIC
            ${FAN_CONTROLLER_ROOT}
    )
    target_link_libraries(fan_controller_lib
        PUBLIC
            pi_controller_lib
            io_driver_lib
            can_driver_lib
    )
    set_module_output_layout(fan_controller_lib "modules/fan_controller")

    add_executable(implement
        sw/apps/implement/main.c
    )
    target_link_libraries(implement PRIVATE fan_controller_lib sil_io_config_lib sil_vcan_config_lib)
    set_module_output_layout(implement "apps/implement")
endif()