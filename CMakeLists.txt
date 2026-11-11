cmake_minimum_required(VERSION 3.16)
project(GalaxyDefender VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(galaxy_defender src/main.c)
target_link_libraries(galaxy_defender PRIVATE raylib)
if(MSVC)
    target_compile_options(galaxy_defender PRIVATE /W4)
else()
    target_compile_options(galaxy_defender PRIVATE -Wall -Wextra -Wpedantic)
endif()
