cmake_minimum_required(VERSION 3.16)

# Zemi R&D 2026 — nRF52 Wearable Firmware Scaffold
# Target: nRF52840 (Zemi wearable)
# Build system: nRF5 SDK + CMake (arm-none-eabi-gcc toolchain)
#
# Quick start
# ───────────
#   export NRF5_SDK_ROOT=/path/to/nRF5_SDK_17.1.0
#   mkdir build && cd build
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=$NRF5_SDK_ROOT/cmake/arm-none-eabi.cmake \
#            -DBOARD=nrf52840dk_nrf52840
#   make -j4

project(zemi_nrf52_scaffold C)

# ── SDK root (required) ──────────────────────────────────────────────
if(NOT DEFINED NRF5_SDK_ROOT)
    message(FATAL_ERROR
        "NRF5_SDK_ROOT not set. "
        "Pass -DNRF5_SDK_ROOT=/path/to/nRF5_SDK_17.1.0 to cmake.")
endif()

# ── Compiler / linker flags ──────────────────────────────────────────
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

add_compile_options(
    ${CPU_FLAGS}
    -std=c11
    -Wall
    -Wextra
    -Wshadow
    -ffunction-sections
    -fdata-sections
    -fno-strict-aliasing
    -O2
    -g3
)

add_link_options(
    ${CPU_FLAGS}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -lm
)

# ── SDK include paths ────────────────────────────────────────────────
set(SDK ${NRF5_SDK_ROOT})

include_directories(
    include

    # nRF5 SDK core
    ${SDK}/components/boards
    ${SDK}/components/drivers_nrf/nrf_soc_nosd
    ${SDK}/components/libraries/util
    ${SDK}/components/libraries/log
    ${SDK}/components/libraries/log/src
    ${SDK}/components/libraries/timer
    ${SDK}/components/toolchain/cmsis/include
    ${SDK}/integration/nrfx
    ${SDK}/integration/nrfx/legacy
    ${SDK}/modules/nrfx
    ${SDK}/modules/nrfx/drivers/include
    ${SDK}/modules/nrfx/hal
    ${SDK}/modules/nrfx/mdk

    # FreeRTOS
    ${SDK}/external/freertos/source/include
    ${SDK}/external/freertos/portable/GCC/nrf52
    ${SDK}/external/freertos/portable/MemMang
)

# ── Compile definitions ──────────────────────────────────────────────
add_compile_definitions(
    NRF52840_XXAA
    BOARD_PCA10056          # nRF52840-DK; change to your board define
    CONFIG_GPIO_AS_PINRESET
    FREERTOS
    NRF_LOG_ENABLED=1
    NRF_LOG_BACKEND_UART_ENABLED=1
    FLOAT_ABI_HARD
)

# ── Source files ─────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.c
    src/mlx90393.c
    src/mpu6050.c
)

# Minimal SDK sources needed (expand as your BSP requires)
set(SDK_SOURCES
    ${SDK}/modules/nrfx/drivers/src/nrfx_twim.c
    ${SDK}/components/libraries/log/src/nrf_log_frontend.c
    ${SDK}/components/libraries/log/src/nrf_log_str_formatter.c
    ${SDK}/components/libraries/log/src/nrf_log_backend_uart.c
    ${SDK}/components/libraries/timer/app_timer_freertos.c
    ${SDK}/external/freertos/source/croutine.c
    ${SDK}/external/freertos/source/event_groups.c
    ${SDK}/external/freertos/source/list.c
    ${SDK}/external/freertos/source/queue.c
    ${SDK}/external/freertos/source/tasks.c
    ${SDK}/external/freertos/source/timers.c
    ${SDK}/external/freertos/portable/GCC/nrf52/port.c
    ${SDK}/external/freertos/portable/MemMang/heap_4.c
    ${SDK}/modules/nrfx/mdk/system_nrf52840.c
)

# ── Linker script ────────────────────────────────────────────────────
# Adjust flash/RAM origin and length for your SoftDevice config.
set(LINKER_SCRIPT ${SDK}/modules/nrfx/mdk/nrf52840_xxaa.ld)
set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")

# ── Target ───────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf ${APP_SOURCES} ${SDK_SOURCES})

# ── Hex / bin output ─────────────────────────────────────────────────
find_program(OBJCOPY arm-none-eabi-objcopy REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
)

# ── Flash target (nrfjprog) ──────────────────────────────────────────
find_program(NRFJPROG nrfjprog)
if(NRFJPROG)
    add_custom_target(flash
        COMMAND ${NRFJPROG} --program ${PROJECT_NAME}.hex --chiperase --verify --reset
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Flashing ${PROJECT_NAME}.hex via nrfjprog"
    )
endif()
