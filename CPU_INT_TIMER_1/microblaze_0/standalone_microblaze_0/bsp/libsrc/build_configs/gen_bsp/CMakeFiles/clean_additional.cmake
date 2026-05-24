# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "D:\\Vivado\\Vitis\\Projects\\CPU_INT_TIMER_1\\microblaze_0\\standalone_microblaze_0\\bsp\\include\\sleep.h"
  "D:\\Vivado\\Vitis\\Projects\\CPU_INT_TIMER_1\\microblaze_0\\standalone_microblaze_0\\bsp\\include\\xiltimer.h"
  "D:\\Vivado\\Vitis\\Projects\\CPU_INT_TIMER_1\\microblaze_0\\standalone_microblaze_0\\bsp\\include\\xtimer_config.h"
  "D:\\Vivado\\Vitis\\Projects\\CPU_INT_TIMER_1\\microblaze_0\\standalone_microblaze_0\\bsp\\lib\\libxiltimer.a"
  )
endif()
