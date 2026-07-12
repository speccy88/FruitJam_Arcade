cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PICO_PLATFORM rp2350)
set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Board type")
option(ENABLE_ADC_AIM "Enable A0/A1 analog arcade aiming for the native rail-shooter" OFF)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# Pico-PIO-USB path must be set BEFORE pico_sdk_init() so TinyUSB creates the target
set(PICO_PIO_USB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Pico-PIO-USB)

include(pico_sdk_import.cmake)

project(fruitjam_arcade C CXX ASM)

pico_sdk_init()

# Optional external-PSRAM heap for future arcade games and larger asset sets.
# Vector Raid currently uses fixed-size pools, so none of these functions are
# called by the game loop yet.
add_library(arcade_psram INTERFACE)
target_sources(arcade_psram INTERFACE
    src/psram_allocator.c
    tlsf/tlsf.c
)
target_include_directories(arcade_psram INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(arcade_psram INTERFACE pico_stdlib hardware_clocks hardware_sync)

# USB host library (PIO-USB, HID, XInput, CDC, MSC).
set(FWUSBHOST_TUSB_DEBUG_LEVEL 2)  # output goes to ring buffer, dumped via 'info'
add_subdirectory(usb-host)

# Native Fruit Jam rail-shooter firmware.
add_executable(fruitjam_railshooter
    src/native_main.cpp
    src/native_game.c
    src/native_pad.c
    src/native_io.c
    src/wire3d.c
    src/dvi.c
    src/gfx.c
    src/input.c
    src/audio.c
    ${FWUSBHOST_TINYUSB_HOST_SOURCES}
)
pico_generate_pio_header(fruitjam_railshooter ${CMAKE_CURRENT_SOURCE_DIR}/src/audio_i2s.pio)
target_include_directories(fruitjam_railshooter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(fruitjam_railshooter PRIVATE
    PICO_STACK_SIZE=0x1000
    ENABLE_ADC_AIM=$<BOOL:${ENABLE_ADC_AIM}>
)
target_link_libraries(fruitjam_railshooter PRIVATE
    arcade_psram fwUSBHost pico_stdlib hardware_pio hardware_spi hardware_sync
    hardware_dma hardware_clocks hardware_i2c hardware_adc
)
pico_enable_stdio_usb(fruitjam_railshooter 1)
pico_enable_stdio_uart(fruitjam_railshooter 0)
pico_add_extra_outputs(fruitjam_railshooter)
