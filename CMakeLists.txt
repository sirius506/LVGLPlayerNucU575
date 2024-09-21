cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#


# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

#set(CMAKE_BUILD_TYPE "Release")

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME DoomPlayerNucU575)

set(LV_CONF_BUILD_DISABLE_EXAMPLES 1)
set(LV_CONF_BUILD_DISABLE_DEMOS 1)
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/BSP/lv_conf.h
     CACHE STRING "" FORCE)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})


# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)
add_subdirectory(CMSIS-DSP/Source)
add_subdirectory(lvgl)
add_subdirectory(btstack)
add_subdirectory(BSP)
add_subdirectory(RTT)
add_subdirectory(chocolate-doom)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Middlewares/Third_Party/ff15/source/diskio.c
    Middlewares/Third_Party/ff15/source/ff.c
    Middlewares/Third_Party/ff15/source/ff_gen_drv.c
    Middlewares/Third_Party/ff15/source/ffsystem.c
    Middlewares/Third_Party/ff15/source/ffunicode.c
    App/app_gui.c
    App/app_main.c
    App/app_a2dp_player.c
    App/app_music_list.c
    App/app_music_main.c
    App/app_osc_music.c
    App/app_sound.c
    App/app_setup.c
    App/app_shell.c
    App/app_verify.c
    App/cover_if.c
    App/doom_helper.c
    App/dualsense.c
    App/dualshock4.c
    App/8bitdozero2.c
    App/fatfs.c
    App/gamepad.c
    App/mix_player.c
    App/sdl_helper.c
    App/sdl_joystick.c
    App/assets/output/Action_Left.c
    App/assets/output/Action_Right.c
    App/assets/output/Arrow_Up.c
    App/assets/output/Arrow_Down.c
    App/assets/output/Arrow_Left.c
    App/assets/output/Arrow_Right.c
    App/assets/output/Arrow_Lower_Left.c
    App/assets/output/Arrow_Lower_Right.c
    App/assets/output/Arrow_Upper_Left.c
    App/assets/output/Arrow_Upper_Right.c
    App/assets/output/Button_Go.c
    App/assets/output/Button_Shutdown.c
    App/assets/output/Button_Stop.c
    App/assets/output/bluetooth_black.c
    App/assets/output/bluetooth_blue.c
    App/assets/output/bluetooth_scan.c
    App/assets/output/volume-control.c
    App/assets/output/brightness.c
    App/assets/img_lv_demo_music_icon_1.c
    App/assets/img_lv_demo_music_icon_2.c
    App/assets/img_lv_demo_music_icon_3.c
    App/assets/img_lv_demo_music_icon_4.c
    App/assets/img_lv_demo_music_corner_left.c
    App/assets/img_lv_demo_music_corner_right.c
    App/assets/img_lv_demo_music_btn_rnd.c
    App/assets/img_lv_demo_music_btn_loop.c
    App/assets/img_lv_demo_music_btn_prev.c
    App/assets/img_lv_demo_music_btn_next.c
    App/assets/img_lv_demo_music_btn_pause.c
    App/assets/img_lv_demo_music_btn_pause_large.c
    App/assets/img_lv_demo_music_btn_play.c
    App/assets/img_lv_demo_music_btn_play_large.c
    App/assets/img_lv_demo_music_wave_top_large.c
    App/assets/img_lv_demo_music_wave_bottom_large.c
    App/assets/img_lv_demo_music_btn_list_play.c
    App/assets/img_lv_demo_music_btn_list_pause.c
    App/assets/img_lv_demo_music_logo.c
    App/assets/img_lv_demo_music_list_border.c
    App/assets/img_lv_demo_music_slider_knob.c
    ntshell/src/lib/core/ntlibc.c
    ntshell/src/lib/core/ntshell.c
    ntshell/src/lib/core/text_editor.c
    ntshell/src/lib/core/text_history.c
    ntshell/src/lib/core/vtrecv.c
    ntshell/src/lib/core/vtsend.c
    ntshell/src/lib/util/ntopt.c
    ntshell/src/lib/util/ntstdio.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${PROJECT_SOURCE_DIR}/lvgl/demos/music
    ${PROJECT_SOURCE_DIR}/Core/Inc
    ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc
    ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32U5xx/Include
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/RTOS2/Include
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/ff15/source
    ${PROJECT_SOURCE_DIR}/CMSIS-DSP-1.15.0/Include
    ${PROJECT_SOURCE_DIR}/SDL
    ${PROJECT_SOURCE_DIR}/btstack
    ${PROJECT_SOURCE_DIR}/btstack/src
    ${PROJECT_SOURCE_DIR}/RTT
    ${PROJECT_SOURCE_DIR}/App
    ${PROJECT_SOURCE_DIR}/BSP
    ${PROJECT_SOURCE_DIR}/Fusion
    ${PROJECT_SOURCE_DIR}/lvgl
    ${PROJECT_SOURCE_DIR}/chocolate-doom-3.0.1/src
    $(PROJECT_SOURCE_DIR)/ntshell/src/lib/core
    $(PROJECT_SOURCE_DIR)/ntshell/src/lib/util
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

target_compile_options(bsp PUBLIC
-DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32U575xx -DARM_MATH_LOOPUNROLL
)

target_compile_options(btstack PUBLIC
-DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32U575xx -DARM_MATH_LOOPUNROLL
)

target_compile_options(CMSISDSP PUBLIC
-DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32U575xx -DARM_MATH_LOOPUNROLL
)

target_compile_options(lvgl INTERFACE
-DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32U575xx -DARM_MATH_LOOPUNROLL
)

target_include_directories(lvgl INTERFACE
    ${PROJECT_SOURCE_DIR}/RTT
    ${PROJECT_SOURCE_DIR}/Core/Inc
    ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc
    ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32U5xx/Include
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
    ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/RTOS2/Include
)

target_include_directories(CMSISDSP PUBLIC
  Drivers/CMSIS/Core/Include
)

target_compile_options(doom PUBLIC
-DUSE_FULL_LL_DRIVER -DUSE_HAL_DRIVER -DSTM32U575xx -DARM_MATH_LOOPUNROLL
)

target_include_directories (doom PUBLIC
  App
  SDL
  BSP
  RTT
  Core/Inc
  lvgl
  chocolate-doom
  ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc
  ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
  ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32U5xx/Include
  ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
  Middlewares/Third_Party/FreeRTOS/Source/include
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
  ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
  ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/RTOS2/Include
  Middlewares/Third_Party/ff15/source
)

target_include_directories(btstack PUBLIC
  App
  BSP
  RTT
  lvgl
  SDL
  btstack
  btstack/src
  btstack/3rd-party/bluedroid/encoder/include
  btstack/3rd-party/bluedroid/decoder/include
  btstack/platform/freertos
  btstack/platform/embedded
  ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc
  ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
  ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32U5xx/Include
  ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
  Core/Inc
  Middlewares/Third_Party/CMSIS/RTOS2/Include
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
  Middlewares/Third_Party/FreeRTOS/Source/include
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    lvgl
    doom
    btstack
    bsp
    rtt
    CMSISDSP
)
