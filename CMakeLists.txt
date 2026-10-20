# =====================================================================
# Embedded_Tools —— 消费侧接入构建（v2.29 P1-1，REQ-10 / N-01）
#
# 定位：本文件是**接入面**，不是门禁面。host 构建与全部质量门仍以 Makefile 为
# 单一入口（计划 §1.3）；二者由 `tools/docsync.sh` 的源列表一致性断言把守，
# 谁漂移谁红（P1-2）。
#
# 单一来源纪律：版本号与 32 个模块开关的**默认值都从 `et_config.h` 读出**，
# 本文件不复制字面量 —— 复制即第二真源。
#
# 用法（host）：
#   cmake -B build-cmake -DCMAKE_BUILD_TYPE=Release -DET_WERROR=ON
#   cmake --build build-cmake --config Release
# 交叉（示例 toolchain 文件在 cmake/toolchains/）：
#   cmake -B build-arm -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-g474.cmake -DET_WERROR=ON
# =====================================================================
cmake_minimum_required(VERSION 3.20)

set(ET_CONFIG_H "${CMAKE_CURRENT_SOURCE_DIR}/et_config.h")
if(NOT EXISTS "${ET_CONFIG_H}")
    message(FATAL_ERROR "et_config.h 未找到: ${ET_CONFIG_H}")
endif()
file(READ "${ET_CONFIG_H}" ET_CONFIG_SRC)

# ---- 从 et_config.h 读一个整数宏（缺失即红，禁静默取默认） ----
function(et_read_int name out)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+([0-9]+)" _m "${ET_CONFIG_SRC}")
    if(_m STREQUAL "")
        message(FATAL_ERROR "et_config.h 中解析不到整数宏 ${name}")
    endif()
    set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

et_read_int(ET_VERSION_MAJOR ET_VER_MAJOR)
et_read_int(ET_VERSION_MINOR ET_VER_MINOR)
et_read_int(ET_VERSION_PATCH ET_VER_PATCH)
math(EXPR ET_VERSION_HEX "(${ET_VER_MAJOR} << 16) | (${ET_VER_MINOR} << 8) | ${ET_VER_PATCH}"
     OUTPUT_FORMAT HEXADECIMAL)

project(et VERSION "${ET_VER_MAJOR}.${ET_VER_MINOR}.${ET_VER_PATCH}" LANGUAGES C)

# ---- 七个库目录（源码与头文件的收集单元；与 Makefile 的 *_SRC 一一对应） ----
set(ET_DIRS core algorithm sys protocol drivers debug storage)

# ---- port 选择：唯一硬件边界，一次只装一个实现 ----
set(ET_PORT "host" CACHE STRING "port/ 下的实现目录名（host | stm32f103 | stm32g474）")
set(ET_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/${ET_PORT}")
if(NOT IS_DIRECTORY "${ET_PORT_DIR}")
    message(FATAL_ERROR "ET_PORT=${ET_PORT} 在 port/ 下不存在（现有: host / stm32f103 / stm32g474）")
endif()

add_library(embedded_tools STATIC)
add_library(et::embedded_tools ALIAS embedded_tools)

foreach(d IN LISTS ET_DIRS)
    file(GLOB _et_srcs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${d}/*.c")
    target_sources(embedded_tools PRIVATE ${_et_srcs})
endforeach()
target_sources(embedded_tools PRIVATE "${ET_PORT_DIR}/port_${ET_PORT}.c")

target_include_directories(embedded_tools
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/port>
        $<BUILD_INTERFACE:${ET_PORT_DIR}>
        $<INSTALL_INTERFACE:include/et>
)
foreach(d IN LISTS ET_DIRS)
    target_include_directories(embedded_tools
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${d}>)
endforeach()

set_target_properties(embedded_tools PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    VERSION   "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}")

# ---- 模块开关：默认值取 et_config.h，显式 -DET_MODULE_X=0 即覆盖 ----
# 语义（计划可裁量项②）：开关只决定 `-DET_MODULE_X=0/1` 是否下发给编译器，
# **所有 .c 一律参与编译**；关闭的模块因此编译成空 TU（头文件内 `#if ET_MODULE_X`
# 屏蔽全部内容）⇒ 目标文件里没有它的任何符号。口径 = "无目标代码与符号"，
# 不是"源文件不参与编译"（计划 HC-8①）。
string(REGEX MATCHALL "#define[ \t]+ET_MODULE_[A-Z0-9_]+[ \t]+[01]+" _et_mod_defs "${ET_CONFIG_SRC}")
foreach(_def IN LISTS _et_mod_defs)
    string(REGEX REPLACE "#define[ \t]+(ET_MODULE_[A-Z0-9_]+)[ \t]+([01]+).*" "\\1;\\2" _pair "${_def}")
    list(GET _pair 0 _mod)
    list(GET _pair 1 _default)
    if(_default MATCHES "^0+$")
        set(_default OFF)
    else()
        set(_default ON)
    endif()
    option(${_mod} "Embedded_Tools 模块开关（默认值取自 et_config.h）" ${_default})
    if(${_mod})
        target_compile_definitions(embedded_tools PUBLIC ${_mod}=1)
    else()
        target_compile_definitions(embedded_tools PUBLIC ${_mod}=0)
    endif()
endforeach()
list(LENGTH _et_mod_defs ET_MODULE_COUNT)
message(STATUS "et: ${PROJECT_VERSION} (ET_VERSION=${ET_VERSION_HEX}) port=${ET_PORT} 模块开关=${ET_MODULE_COUNT}")

# ---- 零警告面：消费侧默认只给 -Wall -Wextra -pedantic，ET_WERROR 才升级 ----
option(ET_WERROR "把警告升级为错误（-Werror）" OFF)
option(ET_BUILD_EXAMPLES "构建 examples/ex_*.c 自检式示例并注册为 CTest 用例" OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang|ARMClang")
    target_compile_options(embedded_tools PRIVATE -Wall -Wextra -pedantic)
    if(ET_WERROR)
        target_compile_options(embedded_tools PRIVATE -Werror)
    endif()
endif()

# ---- flash 几何（host 与板上双几何口径沿用 Makefile 的同名宏） ----
foreach(_g PORT_FLASH_SECTOR_SIZE PORT_FLASH_SECTOR_COUNT PORT_FLASH_ERASE_MS_MAX)
    if(DEFINED ${_g})
        target_compile_definitions(embedded_tools PUBLIC ${_g}=${${_g}})
    endif()
endforeach()

# ---- 可选：六个自检式示例（host only；`make ex` 的 CMake 对应路径） ----
if(ET_BUILD_EXAMPLES AND ET_PORT STREQUAL "host")
    enable_testing()
    file(GLOB ET_EXAMPLE_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex_*.c")
    foreach(src IN LISTS ET_EXAMPLE_SRCS)
        get_filename_component(name "${src}" NAME_WE)
        add_executable(${name} "${src}")
        target_link_libraries(${name} PRIVATE et::embedded_tools)
        add_test(NAME ${name} COMMAND ${name})
    endforeach()
endif()

# ---- 安装与导出（find_package(et CONFIG) 消费形态） ----
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS embedded_tools EXPORT etTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES "${ET_CONFIG_H}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/et")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/port/port.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/et")
# 头文件安装为**扁平** include/et/：本库源码一律按裸名包含（`#include "et_kv.h"`，
# in-tree 靠七个 -I 达成），装成 per-dir 会让 find_package 消费方按文档写法包含失败。
# 七目录 + 三个现役 port 的 35 个头文件 basename 无重名（实测），扁平无歧义。
foreach(d IN LISTS ET_DIRS)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${d}/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/et" FILES_MATCHING PATTERN "*.h")
endforeach()
install(DIRECTORY "${ET_PORT_DIR}/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/et"
        FILES_MATCHING PATTERN "*.h")

install(EXPORT etTargets NAMESPACE et:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/et")
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/etConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/etConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/et")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/etConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/etConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/etConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/et")
