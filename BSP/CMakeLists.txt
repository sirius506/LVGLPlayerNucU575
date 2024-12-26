add_library(bsp STATIC
  bsp_audio.c
  bsp_codec.c
  board_if.c
  bsp_driver_sd.c
  bsp_lcd.c
  bsp_main.c
  bsp_qspi.c
  bsp_touch.c
  bsp_uart_dma.c
  sd_diskio.c
)

target_compile_definitions(bsp PUBLIC 
    USE_FULL_LL_DRIVER 
    USE_HAL_DRIVER 
    STM32U575xx
    ARM_MATH_LOOPUNROLL
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(bsp PUBLIC
     ${PROJECT_SOURCE_DIR}/Core/Inc
     ${PROJECT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc
     ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32U5xx/Include
     ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
     ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/ARM_CMSIS/CMSIS/DSP/Include
     ${PROJECT_SOURCE_DIR}/SEGGER
     ${PROJECT_SOURCE_DIR}/SEGGER/Config
     ${PROJECT_SOURCE_DIR}/FreeRTOS/include
     ${PROJECT_SOURCE_DIR}/FreeRTOS/portable/GCC/ARM_CM33_NTZ/non_secure
     ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/CMSIS_RTOS_V2
     ${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/ff15/source
     ${PROJECT_SOURCE_DIR}/SDL
     ${PROJECT_SOURCE_DIR}/App
     ${PROJECT_SOURCE_DIR}/BSP
     ${PROJECT_SOURCE_DIR}/lvgl
)
