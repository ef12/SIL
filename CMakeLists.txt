cmake_minimum_required(VERSION 3.16)

project(HelloWorldC VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BUILD_PRODUCTION "Build production application targets" ON)
set(ARTIFACTS_ROOT "${CMAKE_SOURCE_DIR}/build/out" CACHE PATH "Output directory for final build artifacts")

set(SW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/sw)
set(UDP_SOCKET_ROOT ${SW_ROOT}/udp_socket)

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
        sw/udp_socket/src/udp_socket.c
    )

    target_include_directories(udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_ROOT}/inc
    )
    target_link_libraries(udp_socket_lib
        PUBLIC
            ${UDP_SOCKET_PLATFORM_LIBS}
    )
    set_module_output_layout(udp_socket_lib "udp_socket")

    add_executable(hello_world
        sw/main.c
    )
    set_module_output_layout(hello_world "app")
endif()