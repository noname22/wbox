cmake_minimum_required(VERSION 3.16)
project(wbox LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

add_executable(wbox
    src/main.c
)

target_link_libraries(wbox PRIVATE SDL3::SDL3)
