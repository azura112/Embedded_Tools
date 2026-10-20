# arm-none-eabi-g474.cmake —— STM32G474VET6 交叉构建工具链描述（v2.29 P1-1）
#
# 用法：
#   cmake -B build-arm -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-g474.cmake -DET_WERROR=ON
#   cmake --build build-arm
#
# 实测口径（计划 HC-8②）：本文件描述"库的交叉编译位形"（静态库产物 + 零警告门），
# 链接脚本/启动文件/固件镜像由消费侧工程承担 —— 本仓库的板侧工程在库外
# `D:\code\STM32CubeMX\G474VET6_ET_TEST\`，其链接形态见 `port/stm32g474/README.md`。

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# 工具链前缀可覆盖（不写死具体安装路径，路径交给 PATH）
set(ET_CROSS_PREFIX "arm-none-eabi-" CACHE STRING "交叉工具链前缀")
set(CMAKE_C_COMPILER   "${ET_CROSS_PREFIX}gcc")
set(CMAKE_AR           "${ET_CROSS_PREFIX}ar")
set(CMAKE_RANLIB       "${ET_CROSS_PREFIX}ranlib")
set(CMAKE_OBJCOPY      "${ET_CROSS_PREFIX}objcopy")
set(CMAKE_SIZE         "${ET_CROSS_PREFIX}size")

# 静态库目标：无 CRT 可链接的裸机环境不必先试编译可执行文件
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -ffunction-sections -fdata-sections")

# 与 port/stm32g474/README.md 的 docbuild 命令同几何（G474 页 2 KB × 16 页参数区）
set(ET_PORT "stm32g474" CACHE STRING "port/ 下的实现目录名")
set(PORT_FLASH_SECTOR_SIZE    2048 CACHE STRING "flash 扇区大小(字节)")
set(PORT_FLASH_SECTOR_COUNT   16   CACHE STRING "参数区扇区数")
set(PORT_FLASH_ERASE_MS_MAX   40   CACHE STRING "单扇区擦除最坏耗时(ms)")
