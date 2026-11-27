cmake_minimum_required(VERSION 3.15)

# =========================================================================
# 1. Toolchain, must be before project()
# =========================================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TOOLCHAIN_PATH "$ENV{HOME}/pub/RISC-V Embedded GCC15" CACHE PATH "WCH toolchain root")
set(TOOLCHAIN_PREFIX "riscv32-wch-elf" CACHE STRING "WCH toolchain executable prefix")

set(_toolchain_suffix "")
if(CMAKE_HOST_WIN32)
    set(_toolchain_suffix ".exe")
endif()
set(_toolchain_bin "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}")

set(CMAKE_C_COMPILER   "${_toolchain_bin}-gcc${_toolchain_suffix}" CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "${_toolchain_bin}-g++${_toolchain_suffix}" CACHE FILEPATH "CXX compiler")
set(CMAKE_ASM_COMPILER "${_toolchain_bin}-gcc${_toolchain_suffix}" CACHE FILEPATH "ASM compiler")

set(CMAKE_OBJCOPY "${_toolchain_bin}-objcopy${_toolchain_suffix}" CACHE FILEPATH "objcopy")
set(CMAKE_OBJDUMP "${_toolchain_bin}-objdump${_toolchain_suffix}" CACHE FILEPATH "objdump")
set(CMAKE_SIZE    "${_toolchain_bin}-size${_toolchain_suffix}" CACHE FILEPATH "size")

set(CMAKE_AR     "${_toolchain_bin}-gcc-ar${_toolchain_suffix}" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${_toolchain_bin}-gcc-ranlib${_toolchain_suffix}" CACHE FILEPATH "ranlib")

foreach(_tool CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_ASM_COMPILER
              CMAKE_OBJCOPY CMAKE_OBJDUMP CMAKE_SIZE CMAKE_AR CMAKE_RANLIB)
    if(NOT EXISTS "${${_tool}}")
        message(FATAL_ERROR "${_tool} not found: ${${_tool}}. Set TOOLCHAIN_PATH or ${_tool}.")
    endif()
endforeach()

# =========================================================================
# 2. Project
# =========================================================================
project(ch32clock LANGUAGES C CXX ASM DESCRIPTION "CH32V203G6U6 clock firmware")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

message(STATUS "WCH CC      = ${CMAKE_C_COMPILER}")
message(STATUS "WCH CXX     = ${CMAKE_CXX_COMPILER}")
message(STATUS "WCH ASM     = ${CMAKE_ASM_COMPILER}")
message(STATUS "WCH OBJCOPY = ${CMAKE_OBJCOPY}")
message(STATUS "WCH OBJDUMP = ${CMAKE_OBJDUMP}")
message(STATUS "WCH SIZE    = ${CMAKE_SIZE}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# =========================================================================
# 3. Target / linker script
# =========================================================================
set(TARGET_NAME "${PROJECT_NAME}")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Ld/Link.ld" CACHE FILEPATH "Firmware linker script")

if(NOT EXISTS "${LINKER_SCRIPT}")
    message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}. Import the MCU SDK or set LINKER_SCRIPT.")
endif()

# =========================================================================
# 4. Flags from native MounRiver Makefile
# =========================================================================
set(ARCH_FLAGS
        -march=rv32imacxw
        -mabi=ilp32
        -msmall-data-limit=8
)

set(COMMON_COMPILE_FLAGS
        ${ARCH_FLAGS}
        -msave-restore
        -fmax-errors=20
        -fmessage-length=0
        -fsigned-char
        -ffunction-sections
        -fdata-sections
        -fno-common
        -Wunused
        -Wuninitialized
)

set(COMMON_LINK_FLAGS
        ${ARCH_FLAGS}
        -msave-restore
        "-T${LINKER_SCRIPT}"
        -nostartfiles
        -Wl,--gc-sections
        "-Wl,-Map,${TARGET_NAME}.map"
        --specs=nano.specs
        --specs=nosys.specs
        -Wl,--print-memory-usage
)

# =========================================================================
# 5. Source files
# =========================================================================
add_executable(${TARGET_NAME}
        main.c
        App/pomodoro.c
        App/display.c
        App/digit_font.c
        App/service.c
        Startup/startup_ch32v20x_D6.S
        User/system_ch32v20x.c
        BSP/GPIO.c
        BSP/systemInit.c

        Peripheral/src/ch32v20x_rcc.c
        Peripheral/src/ch32v20x_gpio.c
        Peripheral/src/ch32v20x_i2c.c
        Peripheral/src/ch32v20x_tim.c
        Peripheral/src/ch32v20x_dma.c
        BSP/I2C.c
        BSP/I2C.h
        BSP/IRQ.c
        BSP/IRQ.h
        Component/oled/oled.c
        Component/oled/oled.h
        Component/oled/oled_ch32.c
        Component/oled/oled_ch32.h
        Component/oled/oledConst.h
        BSP/clocktick.c
        BSP/clocktick.h
        bootLoader/bootCfg.c
        bootLoader/bootCfg.h
        Component/EC11/EC11.c
        Component/EC11/EC11_ch32.c
        Component/EC11/EC11.h
        Component/buzzer/buzzer.c
        Component/buzzer/buzzer_ch32.c
        Component/buzzer/buzzer.h
        BSP/TIM.c
        BSP/TIM.h
)
# =========================================================================
# 6. Include directories
# =========================================================================
target_include_directories(${TARGET_NAME} PRIVATE
        # 工程内跨模块头文件用带目录的路径，如 Component/oled/oled.h。
        "${CMAKE_CURRENT_SOURCE_DIR}"
        # BSP 内部以及 OLED 的 CH32 适配层使用 I2C.h 等本地接口。
        "${CMAKE_CURRENT_SOURCE_DIR}/BSP"
        # WCH 设备头文件及系统时钟头文件沿用 SDK 的短文件名。
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/Core"
        "${CMAKE_CURRENT_SOURCE_DIR}/User"
)

target_compile_definitions(${TARGET_NAME} PRIVATE
        # CH32V203G6U6：与 D6 启动文件及 32KB Flash / 10KB RAM 链接脚本配套。
        CH32V20x_D6
)

# =========================================================================
# 7. Compile and link options
# =========================================================================
target_compile_options(${TARGET_NAME} PRIVATE
        ${COMMON_COMPILE_FLAGS}
        "$<$<CONFIG:Debug>:-O2;-g3>"
        $<$<COMPILE_LANGUAGE:ASM>:-x>
        $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
)

target_link_options(${TARGET_NAME} PRIVATE
        ${COMMON_LINK_FLAGS}
)

# =========================================================================
# 8. Static libraries
# =========================================================================


# =========================================================================
# 9. Post-build outputs: HEX / LST / ASM / ELF copy
# =========================================================================
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}"
        -O ihex
        "$<TARGET_FILE:${TARGET_NAME}>"
        "${TARGET_NAME}.hex"

        COMMAND "${CMAKE_COMMAND}"
        "-DOBJDUMP=${CMAKE_OBJDUMP}"
        "-DELF_FILE=$<TARGET_FILE:${TARGET_NAME}>"
        "-DOUTPUT_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateDisassembly.cmake"

        COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${TARGET_NAME}>"

        COMMAND "${CMAKE_COMMAND}"
        -E copy
        "$<TARGET_FILE:${TARGET_NAME}>"
        "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}.elf"

        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating HEX/LST/ASM files and displaying memory usage..."
        VERBATIM
)

find_program(EANALY_EXE NAMES eanaly HINTS "${TOOLCHAIN_PATH}/bin")
if(EXISTS "${EANALY_EXE}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "${EANALY_EXE}"
            "$<TARGET_FILE:${TARGET_NAME}>"

            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Running WCH post-analysis..."
            VERBATIM
    )
endif()
