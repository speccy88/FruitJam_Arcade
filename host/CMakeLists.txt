cmake_minimum_required(VERSION 3.20)

project(fruitjam_railshooter_host LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GFX_SCALE 2 CACHE STRING "Physical pixels per 128x128 logical game pixel")
if(NOT GFX_SCALE MATCHES "^[1-8]$")
    message(FATAL_ERROR "GFX_SCALE must be an integer from 1 through 8")
endif()

find_package(SDL2 2.0.18 CONFIG QUIET)

if(TARGET SDL2::SDL2)
    set(HOST_SDL_TARGET SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.18)
    set(HOST_SDL_TARGET PkgConfig::SDL2)
endif()
add_executable(fruitjam_railshooter_host
    main.cpp
    host_audio.c
    host_input.c
    host_native_io.c
    ../src/gfx.c
    ../src/native_game.c
    ../src/native_pad.c
    ../src/wire3d.c
)

target_include_directories(fruitjam_railshooter_host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(fruitjam_railshooter_host PRIVATE
    GFX_SCALE=${GFX_SCALE}
    GFX_SKIP_RGB565=1
)

target_link_libraries(fruitjam_railshooter_host PRIVATE ${HOST_SDL_TARGET})

if(NOT MSVC)
    target_compile_options(fruitjam_railshooter_host PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
