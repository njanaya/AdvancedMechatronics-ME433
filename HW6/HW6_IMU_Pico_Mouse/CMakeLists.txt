# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(HW6_IMU_Pico_Mouse C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
cmake_minimum_required(VERSION 3.13)

add_executable(HW6_IMU_Pico_Mouse 
    HW6_IMU_Pico_Mouse.c 
    ssd1306.c
    )

pico_set_program_name(HW6_IMU_Pico_Mouse "HW6_IMU_Pico_Mouse")
pico_set_program_version(HW6_IMU_Pico_Mouse "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(HW6_IMU_Pico_Mouse 1)
pico_enable_stdio_usb(HW6_IMU_Pico_Mouse 0)

target_sources(HW6_IMU_Pico_Mouse PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

# Add the standard library to the build
target_link_libraries(HW6_IMU_Pico_Mouse 
        pico_stdlib 
        pico_unique_id 
        tinyusb_device 
        tinyusb_board
        hardware_i2c
        hardware_uart
        hardware_adc
        )

# Add the standard include files to the build
target_include_directories(HW6_IMU_Pico_Mouse PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lib/ssd1306
)

# Add any user requested libraries
target_link_libraries(HW6_IMU_Pico_Mouse 
        hardware_i2c
        hardware_uart

        )

pico_add_extra_outputs(HW6_IMU_Pico_Mouse)

