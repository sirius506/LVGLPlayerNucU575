add_library(segger STATIC
  SEGGER_RTT.c
  SEGGER_RTT_printf.c
  SEGGER_SYSVIEW.c
  SEGGER_SYSVIEW_FreeRTOS.c
  SEGGER_SYSVIEW_Config_FreeRTOS.c
  SEGGER_RTT_ASM_ARMv7M.S
)

target_include_directories(segger PUBLIC
  ${PROJECT_SOURCE_DIR}/Core/Inc
  ${PROJECT_SOURCE_DIR}/FreeRTOS/include
  ${PROJECT_SOURCE_DIR}/FreeRTOS/portable/GCC/ARM_CM33_NTZ/non_secure
  ${PROJECT_SOURCE_DIR}/SEGGER
  ${PROJECT_SOURCE_DIR}/SEGGER/Config
)
