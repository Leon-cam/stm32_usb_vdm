cmake_minimum_required(VERSION 3.22)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/arm-none-eabi.cmake)

project(nucleo-g071rb-usbpd C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(MCU_FAMILY STM32G0xx)
set(MCU_MODEL  STM32G071xx)
set(CPU_FLAGS  -mcpu=cortex-m0plus -mthumb)

# --- Core sources (CubeMX generated) ---
file(GLOB CORE_SOURCES
    Core/Src/main.c
    Core/Src/stm32g0xx_hal_msp.c
    Core/Src/stm32g0xx_it.c
    Core/Src/app_freertos.c
    Core/Src/system_stm32g0xx.c
)

# --- HAL driver ---
file(GLOB_RECURSE HAL_SOURCES
    Drivers/STM32G0xx_HAL_Driver/Src/*.c
)
# Remove template files that should not be compiled
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*_template\\.c$")

# --- USBPD Middleware (ST library sources) ---
file(GLOB_RECURSE USBPD_MW_SOURCES
    Middlewares/ST/STM32_USBPD_Library/Core/src/*.c
    Middlewares/ST/STM32_USBPD_Library/Devices/STM32G0XX/src/*.c
)
list(FILTER USBPD_MW_SOURCES EXCLUDE REGEX ".*_template\\.c$")
list(FILTER USBPD_MW_SOURCES EXCLUDE REGEX ".*usbpd_trace\\.c$")
list(FILTER USBPD_MW_SOURCES EXCLUDE REGEX ".*/Core/src/usbpd_dpm_core\\.c$")

# --- USBPD Application & Target (CubeMX generated) ---
file(GLOB USBPD_APP_SOURCES
    USBPD/App/*.c
    USBPD/Target/*.c
)

# --- TCPP Software Pack (CubeMX generated) ---
file(GLOB TCPP_SOURCES
    TCPP/App/*.c
)

# --- BSP (Board Support Package) ---
set(BSP_SOURCES
    Drivers/BSP/X-NUCLEO-SRC1M1/src1m1_usbpd_pwr.c
    Drivers/BSP/X-NUCLEO-SRC1M1/src1m1_bus.c
    Drivers/BSP/Components/tcpp0203/tcpp0203.c
    Drivers/BSP/Components/tcpp0203/tcpp0203_reg.c
)

# --- FreeRTOS (GCC port for Cortex-M0+) ---
set(FREERTOS_SOURCES
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0/port.c
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
)

# --- Startup (GCC) ---
set(STARTUP_FILE
    Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/gcc/startup_stm32g071xx.s
)

# --- Include Paths ---
set(INCLUDE_DIRS
    Core/Inc
    USBPD/App
    USBPD/Target
    TCPP/App
    TCPP/Target
    Drivers/CMSIS/Device/ST/STM32G0xx/Include
    Drivers/CMSIS/Include
    Drivers/CMSIS/Core/Include
    Drivers/STM32G0xx_HAL_Driver/Inc
    Drivers/STM32G0xx_HAL_Driver/Inc/Legacy
    Drivers/BSP/X-NUCLEO-SRC1M1
    Drivers/BSP/Components/tcpp0203
    Middlewares/ST/STM32_USBPD_Library/Core/inc
    Middlewares/ST/STM32_USBPD_Library/Devices/STM32G0XX/inc
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
)

# --- Linker Script ---
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32G071RBTX_FLASH.ld)

# --- Compile Definitions ---
add_compile_definitions(
    USE_HAL_DRIVER
    USE_FULL_LL_DRIVER
    ${MCU_MODEL}
    USBPD_PORT_COUNT=1
    _SRC
    _VDM
    _RTOS
    USBPDCORE_LIB_PD3_FULL
)

# --- Build Target ---
add_executable(${PROJECT_NAME}.elf
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${USBPD_MW_SOURCES}
    ${USBPD_APP_SOURCES}
    ${TCPP_SOURCES}
    ${BSP_SOURCES}
    ${FREERTOS_SOURCES}
    ${STARTUP_FILE}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INCLUDE_DIRS})

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fdata-sections
    -ffunction-sections
    $<$<CONFIG:Debug>:-Og -g3 -ggdb>
    $<$<CONFIG:Release>:-O2>
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_USBPD_Library/Core/lib/USBPDCORE_PD3_FULL_CM0PLUS_wc32.a
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    --specs=nano.specs
    --specs=nosys.specs
)

# --- Post-build: generate .bin and .hex ---
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN"
)
