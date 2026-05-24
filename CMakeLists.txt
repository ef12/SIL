cmake_minimum_required(VERSION 3.16)

project(HelloWorldC VERSION 1.0 LANGUAGES C)

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

    add_executable(can_app1
        sw/apps/can_app1/main.c
    )
    target_link_libraries(can_app1 PRIVATE can_transport_udp_lib)
    set_module_output_layout(can_app1 "apps/can_app1")

    add_executable(can_app2
        sw/apps/can_app2/main.c
    )
    target_link_libraries(can_app2 PRIVATE can_transport_udp_lib)
    set_module_output_layout(can_app2 "apps/can_app2")

    add_executable(hello_world
        sw/apps/main.c
    )
    set_module_output_layout(hello_world "apps")
endif()