cmake_minimum_required(VERSION 3.16)

project(HelloWorldC VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(hello_world
    src/main.c
)