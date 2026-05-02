cmake_minimum_required(VERSION 3.16)

project(HelloWorldC VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BUILD_PRODUCTION "Build production application targets" ON)
set(ARTIFACTS_ROOT "${CMAKE_SOURCE_DIR}/build/out" CACHE PATH "Output directory for final build artifacts")

set(SW_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/sw)
set(GREETER_ROOT ${SW_ROOT}/greeter)
set(HELLO_OUTPUT_ROOT ${SW_ROOT}/hello_output)
set(UDP_SOCKET_ROOT ${SW_ROOT}/udp_socket)

set(UDP_SOCKET_PLATFORM_LIBS)
if(WIN32)
    list(APPEND UDP_SOCKET_PLATFORM_LIBS ws2_32)
endif()

include(CTest)
include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

if(BUILD_PRODUCTION AND BUILD_TESTING)
    message(FATAL_ERROR "BUILD_PRODUCTION and BUILD_TESTING cannot both be ON in the same build folder. Use separate build trees, for example build/prod and build/tests.")
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

    add_library(hello_output_lib
        sw/hello_output/src/hello_output.c
    )

    target_include_directories(hello_output_lib
        PUBLIC
            ${HELLO_OUTPUT_ROOT}/inc
    )
    set_module_output_layout(hello_output_lib "hello_output")

    add_library(greeter_lib
        sw/greeter/src/greeter.c
    )

    target_include_directories(greeter_lib
        PUBLIC
            ${GREETER_ROOT}/inc
            ${HELLO_OUTPUT_ROOT}/inc
    )
    target_link_libraries(greeter_lib
        PUBLIC
            hello_output_lib
    )
    set_module_output_layout(greeter_lib "greeter")

    add_executable(hello_world
        sw/main.c
    )

    target_include_directories(hello_world
        PRIVATE
            ${GREETER_ROOT}/inc
    )
    target_link_libraries(hello_world
        PRIVATE
            greeter_lib
    )
    set_module_output_layout(hello_world "app")
endif()

if(BUILD_TESTING)
    find_program(RUBY_EXECUTABLE NAMES ruby REQUIRED)

    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.6.1
    )

    FetchContent_Declare(
        cmock
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/CMock.git
        GIT_TAG v2.6.0
    )

    FetchContent_GetProperties(unity)
    if(NOT unity_POPULATED)
        FetchContent_Populate(unity)
    endif()

    FetchContent_GetProperties(cmock)
    if(NOT cmock_POPULATED)
        FetchContent_Populate(cmock)
    endif()

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/test/cmock.yml.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmock.yml
        @ONLY
    )

    function(add_cmock_mock output_var header_path)
        get_filename_component(header_name "${header_path}" NAME_WE)
        set(mock_dir "${CMAKE_CURRENT_BINARY_DIR}/test/generated/mocks")
        set(mock_source "${mock_dir}/Mock${header_name}.c")
        set(mock_header "${mock_dir}/Mock${header_name}.h")

        add_custom_command(
            OUTPUT
                "${mock_source}"
                "${mock_header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${mock_dir}"
            COMMAND ${RUBY_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/test/generate_mock.rb"
                "${cmock_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/cmock.yml"
                "${CMAKE_CURRENT_SOURCE_DIR}/${header_path}"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/test/generate_mock.rb"
                "${CMAKE_CURRENT_BINARY_DIR}/cmock.yml"
                "${CMAKE_CURRENT_SOURCE_DIR}/${header_path}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            VERBATIM
        )

        set(${output_var} "${mock_source}" PARENT_SCOPE)
    endfunction()

    function(add_unity_cmock_test)
        set(options)
        set(oneValueArgs TARGET TEST_FILE TEST_NAME OUTPUT_PATH)
        set(multiValueArgs SOURCES INCLUDE_DIRS MOCK_HEADERS LINK_LIBS)
        cmake_parse_arguments(AUCT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        if(NOT AUCT_TARGET OR NOT AUCT_TEST_FILE OR NOT AUCT_TEST_NAME OR NOT AUCT_OUTPUT_PATH)
            message(FATAL_ERROR "add_unity_cmock_test requires TARGET, TEST_FILE, TEST_NAME, and OUTPUT_PATH")
        endif()

        set(AUCT_MOCK_SOURCES "")
        foreach(mock_header IN LISTS AUCT_MOCK_HEADERS)
            add_cmock_mock(GENERATED_MOCK_SOURCE ${mock_header})
            list(APPEND AUCT_MOCK_SOURCES ${GENERATED_MOCK_SOURCE})
        endforeach()

        add_executable(${AUCT_TARGET}
            ${AUCT_TEST_FILE}
            ${AUCT_SOURCES}
            ${AUCT_MOCK_SOURCES}
            ${unity_SOURCE_DIR}/src/unity.c
            ${cmock_SOURCE_DIR}/src/cmock.c
        )

        target_include_directories(${AUCT_TARGET}
            PRIVATE
                ${AUCT_INCLUDE_DIRS}
                ${unity_SOURCE_DIR}/src
                ${cmock_SOURCE_DIR}/src
                ${CMAKE_CURRENT_BINARY_DIR}/test/generated/mocks
        )

        if(AUCT_LINK_LIBS)
            target_link_libraries(${AUCT_TARGET}
                PRIVATE
                    ${AUCT_LINK_LIBS}
            )
        endif()

        set_module_output_layout(${AUCT_TARGET} ${AUCT_OUTPUT_PATH})

        add_test(
            NAME ${AUCT_TEST_NAME}
            COMMAND ${AUCT_TARGET}
        )
    endfunction()

    add_unity_cmock_test(
        TARGET greeter_tests
        TEST_FILE sw/greeter/test/test_greeter.c
        TEST_NAME greeter_unit_tests
        OUTPUT_PATH greeter/test
        SOURCES sw/greeter/src/greeter.c
        INCLUDE_DIRS ${GREETER_ROOT}/inc ${HELLO_OUTPUT_ROOT}/inc
        MOCK_HEADERS sw/hello_output/inc/hello_output.h
    )

    add_unity_cmock_test(
        TARGET hello_output_tests
        TEST_FILE sw/hello_output/test/test_hello_output.c
        TEST_NAME hello_output_unit_tests
        OUTPUT_PATH hello_output/test
        SOURCES sw/hello_output/src/hello_output.c
        INCLUDE_DIRS ${HELLO_OUTPUT_ROOT}/inc
    )

    add_unity_cmock_test(
        TARGET udp_socket_tests
        TEST_FILE sw/udp_socket/test/test_udp_socket.c
        TEST_NAME udp_socket_unit_tests
        OUTPUT_PATH udp_socket/test
        SOURCES sw/udp_socket/src/udp_socket.c
        INCLUDE_DIRS ${UDP_SOCKET_ROOT}/inc
        LINK_LIBS ${UDP_SOCKET_PLATFORM_LIBS}
    )
endif()