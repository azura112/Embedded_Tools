# arm-none-eabi-f103.cmake —— STM32F103C8T6(BluePill) 交叉构建工具链描述（v2.29 P1-1）
#
# 用法：
#   cmake -B build-arm-f103 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-f103.cmake -DET_WERROR=ON
#   cmake --build build-arm-f103
#
# f103 参数区 = 1 KB × 8 页 = `port/port.h` 的编译期默认几何 ⇒ 本文件不覆盖
# `PORT_FLASH_*`（与 `port/stm32f103/README.md` 的命令 1/2 同位形：无 flash 宏）。

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

set(ET_CROSS_PREFIX "arm-none-eabi-" CACHE STRING "交叉工具链前缀")
set(CMAKE_C_COMPILER   "${ET_CROSS_PREFIX}gcc")
set(CMAKE_AR           "${ET_CROSS_PREFIX}ar")
set(CMAKE_RANLIB       "${ET_CROSS_PREFIX}ranlib")
set(CMAKE_OBJCOPY      "${ET_CROSS_PREFIX}objcopy")
set(CMAKE_SIZE         "${ET_CROSS_PREFIX}size")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m3 -mthumb -ffunction-sections -fdata-sections")

set(ET_PORT "stm32f103" CACHE STRING "port/ 下的实现目录名")
