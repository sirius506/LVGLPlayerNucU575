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
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(bsp PUBLIC
    ../Core/Inc
    ../Drivers/STM32U5xx_HAL_Driver/Inc
    ../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
    ../Drivers/CMSIS/Device/ST/STM32U5xx/Include
    ../Drivers/CMSIS/Include
    ../Middlewares/Third_Party/ARM_CMSIS/CMSIS/DSP/Include
    ../SEGGER
    ../SEGGER/Config
    ../FreeRTOS/include
    ../FreeRTOS/portable/GCC/ARM_CM33_NTZ/non_secure
    ../Middlewares/Third_Party/CMSIS_RTOS_V2
    ../Middlewares/Third_Party/ff15/source
    ../SDL
    ../App
    ../BSP
    ../lvgl
)
