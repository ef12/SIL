cmake_minimum_required(VERSION 3.16)
project(sil_consumer C)

find_package(SilLib REQUIRED)

add_executable(sil_consumer main.c)
target_link_libraries(sil_consumer PRIVATE SilLib::sil_lib)
