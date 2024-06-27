add_library(rtt STATIC
  SEGGER_RTT.c
  SEGGER_RTT_printf.c
)

target_include_directories(rtt PUBLIC
  ${PROJECT_SOURCE_DIR}/RTT
)
