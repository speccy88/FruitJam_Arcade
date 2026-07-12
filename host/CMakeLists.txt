cmake_minimum_required(VERSION 3.20)

project(fruitjam_railshooter_host LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG QUIET)

if(TARGET SDL2::SDL2)
    set(HOST_SDL_TARGET SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
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
    WILI8JAM_ENABLE_LUA_BINDINGS=0
)

target_link_libraries(fruitjam_railshooter_host PRIVATE ${HOST_SDL_TARGET})

if(NOT MSVC)
    target_compile_options(fruitjam_railshooter_host PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
