# 从零到板上 —— Embedded_Tools 快速上手 (v2.1)

> 目标: 用**一条命令一步**的节奏，把库裁成自己的、跑通 host 单测、移植到新 MCU、上板看串口。
> 全程 C99、零动态内存; 每步都有可复现命令与"该看到什么"。
> 相关深读: 接口细节 [API_GUIDE.md](API_GUIDE.md) ｜ 平台移植 [port/_template/README.md](../port/_template/README.md) ｜ 发布纪律 [README](../README.md) checklist。

---

## 0. 环境准备 (一次性)

| 用途 | 工具 | 验证命令 |
|---|---|---|
| host 单测 | gcc (C99) | `gcc --version` |
| ARM 交叉 | GNU Tools for STM32 13.3.rel1 | `arm-none-eabi-gcc --version` |
| 烧录 | STM32CubeProgrammer CLI + ST-LINK | `STM32_Programmer_CLI --version` |
| (可选) 构建 | MinGW-w64 的 `mingw32-make` | `mingw32-make --version` |
| 板侧上位机脚本（v2.28 P3-2 显式声明） | **Python 3.12.4** + **pyserial 3.5** | `python -c "import serial.tools.list_ports"` |

**板侧脚本依赖面（v2.28 P0/P3-2，`REQ-5` 派生 + README 清单⑥ 环境注记纪律）**：`tools/board_window.py`（板侧窗口一键化）、
`tools/xmodem_send.py`、`tools/modbus_master.py`、`tools/pack_image.py`（打包面仅标准库）均需 Python 3 + **pyserial**
（前三件用 `serial` / `serial.tools.list_ports`）。本机实测 = Python **3.12.4** + pyserial **3.5**；
此前该依赖只载于 v1.6/v1.8 历史交付档的环境注记，现役 README/docs 零声明 ⇒ 本版补齐（依赖未声明即成失实声称）。
这些脚本**不进 CI**（`.github/workflows/*` 零 `tools/*.py`，实测）——板侧动作依赖物理在位，CI 无探针不可跑。

Windows Git Bash 下没有 `make` 也能用 `mingw32-make`; 或直接照 README 的纯 gcc 命令。

## 0.5 十分钟最小样机 (ringbuf + kv, v2.29 P0-3)

目标 = 干净机器上**十分钟内**看到"库能用"的两块证据：`core/et_ringbuf`（字节环形缓冲，零动态内存）
与 `storage/et_kv`（掉电安全参数区，host 侧由 `port_host` 虚拟 flash 承载）。载体就是仓库自带的
第 6 个配方示例 `examples/ex_min.c`，它自检式断言、退出码 0 = PASS。

| 步 | 命令（Windows 用 `mingw32-make`，Linux/macOS 用 `make`；以下均在仓库根执行） | 该看到什么 |
|---|---|---|
| 1 一键跑全部配方 | `make ex` | 六个示例各出一行 `[ex_*] PASS`，末行即 `[ex_min] PASS`，退出码 0（任一 FAIL 即红，CI 常设） |
| 2 只跑这一例（不依赖 make） | `mkdir -p build && gcc -std=c99 -Wall -Wextra -pedantic -I. -Icore -Iprotocol -Istorage -Iport -Iport/host examples/ex_min.c core/et_ringbuf.c protocol/et_crc.c storage/et_kv.c port/host/port_host.c -o build/ex_min.exe && ./build/ex_min.exe` | 五个源文件即最小闭环（示例 + ringbuf + crc + kv + host port）；编译零警告、输出同下方预期、退出码 0 |
| 3 全库回归 | `make WERROR=1 test` | 末尾 `=== RESULT: ALL PASS (fail=0) ===`（526 例）；`-Wall -Wextra -pedantic -Werror` 下零警告（gcc 口径，见 README 支持矩阵） |
| 4 验证"关掉的模块没有符号" | 关：`gcc -std=c99 -DET_MODULE_CRC=0 -I. -Iprotocol -c protocol/et_crc.c -o build/crc_off.o && nm build/crc_off.o > build/crc_off.syms`；开：`gcc -std=c99 -I. -Iprotocol -c protocol/et_crc.c -o build/crc_on.o && nm build/crc_on.o > build/crc_on.syms`；读数：`grep -c " T et_" build/crc_off.syms build/crc_on.syms` | 本机实测 **off=0 / on=8**（口径 = 无目标代码/符号，不是"源文件不参与编译"）；CMake 侧同形态读数见下方「CMake 接入」小节（`-DET_MODULE_KV=0` 实测归档符号 10 ↔ 0） |
| 5 改自己的工程 | 复制 `examples/ex_min.c` 的两个 API 调用块（init/format/set/get）进你的 main | 5~10 行即可，无需改库源码 |

预期输出（逐字，`make ex` 的该配方）：

```text
[ex_min] start
  ringbuf: cap=16 wrote=7 read=7 bytes match
  ringbuf: full-path short write 1 of 15 (free=1), empty read 0
  kv: format+init ok, keys=2
  kv: read-back match (k=1 len=4, k=2 len=11)
  kv: after re-init without format (simulated reboot) k=1 len=4 first byte=0xA0
  kv: overwrite persisted (k=1 len=2, first byte=0x5A)
[ex_min] PASS
```

常见失败与自查（三类，环境注记精确到版本）：

- **编译器**：`make`（Windows 无 `make` 时）→ 用 `mingw32-make`；本机实测 gcc **16.1.0**（MinGW-w64）与 clang **23.1.0**（LLVM）；ARM 交叉 = GNU Tools for STM32 **13.3.rel1**。ARM 工具链不在 PATH 时 port 构建与 `sizecheck` 会红，先 `arm-none-eabi-gcc --version` 自查。
- **内存门（ASan）本机可达性**：gcc 侧 `make test-asan` 报 `cannot find -lasan`（MinGW 不带 libasan）；clang 侧一条命令即可，**无需手工改 PATH**——ASan 运行时目录交给 Makefile 变量（v2.29 P3-3①，拷贝到产物旁）：
  `make test-asan CC=clang ASAN_DLL_DIR=D:/software/LLVM/lib/clang/23/lib/windows`（`test-asan-1k` 同形态；两目标实测 `=== RESULT: ALL PASS (fail=0) ===`；口径与警告面见 README 支持矩阵「内存检查门」行）。
- **字符集与路径空格**：串口/日志读用 UTF-8 终端；工程路径含空格时 `make` 目标可能被拆分，建议无空格路径；Windows Defender/占用会令 `build/*.exe` 链接失败，先关闭上一个 `ex_*.exe` 实例。
- **平台支持面**：能不能用你的板子/编译器/RTOS，只看 README 的「支持矩阵」节（验证过 / 未验证 / 不支持三档，逐行带验证方式），不推测。

---

## 0.9 CMake 接入 (v2.29 P1-1/P1-3, N-01 / REQ-10)

仓库根 `CMakeLists.txt` 把本库导出为 **`et::embedded_tools`**，消费侧只需要 5 行。
两条路径各管自己：**Makefile 仍是本仓库 host 门禁与测试的单一入口**（`make test` 那套），
CMake 是给"要把本库装进现有工程"的人用的接入面；两边的源文件列表由
`tools/docsync.sh` 的一致性断言把守（谁漂移谁红）。

**形态 A —— `add_subdirectory`（源码随工程走）**，`CMakeLists.txt` 全文 5 行：

```cmake
cmake_minimum_required(VERSION 3.20)
project(et_ac6_subdir C)
add_subdirectory("${ET_SRC}" et_build)          # ET_SRC = 本仓库根
add_executable(ac6_sub main.c)
target_link_libraries(ac6_sub PRIVATE et::embedded_tools)
```

**形态 B —— `find_package`（先装再用）**，同样 5 行：

```cmake
cmake_minimum_required(VERSION 3.20)
project(et_ac6_pkg C)
find_package(et CONFIG REQUIRED)
add_executable(ac6_pkg main.c)
target_link_libraries(ac6_pkg PRIVATE et::embedded_tools)
```

安装与消费（`etConfig.cmake` + export set 由 `cmake/etConfig.cmake.in` 生成）：

```sh
cmake -S . -B build-cmake -DCMAKE_INSTALL_PREFIX=<前缀>   # 配
cmake --build build-cmake                                  # 建
cmake --install build-cmake                                # 装
cmake -S . -B b -DCMAKE_PREFIX_PATH=<前缀>                 # 消费侧配置
```

| 项 | 形态 | 说明 |
|---|---|---|
| 语言标准 | `C_STANDARD 99` + `C_EXTENSIONS OFF` | 与 Makefile 的 `-std=c99 -pedantic` 同位形 |
| 模块开关 | `-DET_MODULE_KV=0`（32 个同名 option） | **默认值从 `et_config.h` 读出**，不在 CMake 里复制第二份；显式给 0/1 才覆盖 |
| 关模块的口径 | 所有 `.c` 仍参与编译，关掉的编译成**空 TU** | 实测 `-DET_MODULE_KV=0` 后 `et_kv.c.obj` 的 `et_` 定义符号 = **0**（默认位形 10），`nm` 可核（计划 HC-8①：只声称"无目标代码与符号"，不声称"不参与编译"） |
| 零警告门 | `-DET_WERROR=ON` | 加 `-Wall -Wextra -pedantic -Werror`；host 与两板交叉均实测 0 警告 |
| port 选择 | `-DET_PORT=host\|stm32f103\|stm32g474` | 一次只装一个实现；目录不存在直接 `FATAL_ERROR`（不静默退回首项） |
| flash 几何 | `-DPORT_FLASH_SECTOR_SIZE=2048` 等三枚 | 传下去即 PUBLIC 定义；不传 = 用 `port.h` 的编译期默认（f103 形态） |
| 示例 | `-DET_BUILD_EXAMPLES=ON`（host 限定） | 六个 `examples/ex_*.c` 各注册为一条 CTest 用例，`ctest` 实测 6/6 Passed |

交叉编译（toolchain 文件描述位形，不写死工具链路径）：

```sh
cmake -B build-arm -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-g474.cmake -DET_WERROR=ON
cmake --build build-arm     # 产物 = libembedded_tools.a（裸机环境按静态库试编译）
cmake -B build-arm-f103 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-f103.cmake -DET_WERROR=ON
cmake --build build-arm-f103
```

常见失败：

- **`ET_PORT=xxx 在 port/ 下不存在`**：拼写或大小写；现役三套 = `host` / `stm32f103` / `stm32g474`（`ch32x035` 为私有工程，不在接入面）。
- **`find_package(et CONFIG)` 找不到包**：没给 `CMAKE_PREFIX_PATH`，或 `<前缀>` 里没 `lib/cmake/et/etConfig.cmake`（`cmake --install` 未跑）。
- **头文件包含不到**：安装形态是**扁平** `include/et/`（本库按裸名包含，`#include "et_kv.h"`）；把 `et/` 再套一层目录是自己改了安装前缀结构。
- **交叉报 `no CMAKE_C_COMPILER`**：`arm-none-eabi-gcc` 不在 PATH；工具链文件只写前缀 `arm-none-eabi-`，路径交给你（可用 `-DET_CROSS_PREFIX=` 换前缀）。
- **关模块后链接期报未定义符号**：你的代码仍在调用被关模块——这正是裁剪该有的失败（见 §1）。

---

## 1. 裁剪: 只留你要的模块

```sh
# 编辑 et_config.h, 把不用的 ET_MODULE_* 置 0 (或用 -D 覆盖, 不动源文件):
gcc -DET_MODULE_SELFTEST=1 -DET_MODULE_KV=1 ...   # 例: 只启用自测与 KV
```

**规则**: 开关置 0 后对应 `.c` 移出构建列表(头文件内容亦被屏蔽)。最小系统 = 你实际调用的模块;
`ET_MODULE_SELFTEST` 默认 0(发布裁剪)，host 测试与板上自测固件用 `-DET_MODULE_SELFTEST=1` 打开。

**该看到什么**: 编译源清单里不再出现被裁掉模块的 `.c`; 引用被裁模块的代码会编译失败——这是有意的
（防止"关了开关却漏删调用"的静默错配）。

## 2. host 单测: 先证明算法对，再谈硬件

```sh
cd Embedded_Tools
mingw32-make test        # 或 make test
```

**该看到什么**: 末尾 `=== RESULT: ALL PASS (fail=0) ===`，共 **526** 例。

```sh
mingw32-make test-g4     # 双几何回归 (G474 2KB 页几何; storage 布局改动必跑)
mingw32-make test-tab    # shell Tab 补全形态 (ET_SHELL_TAB=1) 独立矩阵
mingw32-make bench       # host 基准 (数字入 docs/bench.md, 须附环境注记)
mingw32-make ex          # 配方可执行载体自检 (11.10/11.11/升级流程, v2.3)
```

没有 `make` 时，直接照 [README](../README.md) "快速开始" 里的纯 gcc 命令复制粘贴即可。

## 3. 移植到你的 MCU: 填 port 契约

库只有一个硬件边界——`port/port.h`。复制模板目录开始:

```sh
cp -r port/_template port/stm32f103        # 换成你的平台名
# 按 port/_template/README.md 填空: port_tick_get_ms / port_putc /
#   port_critical_enter·exit / port_flash_read·write·erase_sector / (可选) port_wdt_*
```

**几何三宏**（flash 参数区）必须按芯片覆盖，缺了会被 `#error` 守卫挡住(防"忘了 -D 静默错配"):

```sh
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c99 -Wall -Wextra -pedantic -Os \
  -DPORT_FLASH_SECTOR_SIZE=2048 -DPORT_FLASH_SECTOR_COUNT=16 -DPORT_FLASH_ERASE_MS_MAX=40 \
  -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/stm32g474 \
  -c port/stm32g474/port_stm32g474.c -o /dev/null
```

**该看到什么**: 零警告通过; 去掉任一 `-D` 立即 `#error` 失败(正是守卫在起作用)。

**移植验收顺序**(摘自模板 checklist): 填空 → 几何守卫 → 链接脚本挖走参数区 + `ASSERT` 兜底 →
host 侧以本平台几何跑 `make test` → 零警告交叉编译 → README 写 ` ```docbuild ` 定界构建块 →
flash 契约自测 → 上板最小 demo。完整清单见 [port/_template/README.md](../port/_template/README.md)。

## 4. 串口调试: shell + AT 命令

裸机 demo 的常规骨架: **UART RX 中断写 `et_ringbuf`**（不要在睡眠窗口里轮询 RX —— 115200 下字节间隔
87µs < 1ms 睡眠窗口，必丢包，见 API_GUIDE 11.8），主循环排空环 → `et_atcmd`/`et_shell` 解析。

```c
static et_ringbuf_t rx_rb;  static uint8_t rx_mem[128];
et_ringbuf_init(&rx_rb, rx_mem, sizeof(rx_mem));

void USART1_IRQHandler(void) {                  /* ISR: 只管写 */
    uint8_t ch = (uint8_t)(USART1->DR & 0xFFu);
    (void)et_ringbuf_write(&rx_rb, &ch, 1u);
}

/* 主循环: 排空环 -> 喂 shell (atcmd 之上, 自带回显/help) */
uint8_t b;
while (et_ringbuf_read(&rx_rb, &b, 1u) == 1u) {
    (void)et_shell_feed(&sh, b);
}
```

串口助手接 115200-8N1，敲 `help` 回车。

**该看到什么**: 命令行回显 + `help` 列出你注册的 AT 命令; 未注册命令返回错误而非死机。

## 5. 板上自测: 一条命令冒烟

在 `et_config.h`（或 `-D`）打开 `ET_MODULE_SELFTEST=1`，固件里跑一遍内建套件，报告经串口回读。

```c
#if ET_MODULE_SELFTEST
et_selftest_run_all(my_report_cb, NULL);        /* 20 套件: ringbuf/filter/pid/... kv/bootctl */
#endif
```

或挂到 AT 命令上（参考 F103/G474 demo 的 `AT+SELFTEST`）。

**该看到什么**: 逐套件 `PASS` 与末行 `SELFTEST: 20/20`（`ET_MODULE_KV=1` 且已 init 时才跑 kv/bootctl
破坏性套件）。host 侧同一组件会复跑一遍，板上/PC 结果可直接比对。

## 6. 闭环控制示例 (v2.1 新模块)

"测量 → 滤波 → 控制 → 输出"四段齐了: `et_filter` → `et_pid` → `et_spwm`，`et_stats` 旁路判稳。

```c
et_lpf1_init(&lpf, 8192u);                                  /* 去噪(微分前置滤波) */
et_pid_cfg_t cfg = { .kp=19660, .ki=4915, .kd=1640,
                     .out_min=0, .out_max=1000, .i_min=-300, .i_max=300,
                     .d_on_measure=1 };
et_pid_init(&pid, &cfg);

/* 固定 10ms 周期任务: */
int32_t pv  = et_lpf1_update(&lpf, adc_read());
int32_t out = et_pid_step(&pid, 500, pv, 10u);
et_spwm_set(0u, (uint32_t)out);
et_stats_push(&st, pv);
```

整定步骤（先 P 后 I 再 D、Ziegler-Nichols 定性、抗饱和与常见坑）见
[API_GUIDE 11.10](API_GUIDE.md#1110-定点闭环整定配方et_lpf1--et_pid--et_spwm--et_statsv21)。

## 7. 提交前自查 (与发布 checklist 对齐)

```sh
sh tools/docsync.sh              # 文档同步断言 (含 apidump --check)
sh tools/apidump.sh --diff       # 公开面 vs v2.0 冻结基线: 必须"纯新增"
sh tools/sizecheck.sh            # ARM 体积表与实测逐值比对 (需 arm-none-eabi-size)
```

**该看到什么**: `docsync: pass=N fail=0`; `apidump: OK —— 纯新增`; `sizecheck: pass=3 fail=0`。

**改并发用法前先查**：[`docs/concurrency.md`](concurrency.md)（35 模块 × 4 列：ISR 可调用 / 任务可调用 /
单所有者 / 多生产者是否安全，每行带 `文件:行` 的头注原文）。判某 API 能否进中断以前**先看这一行**，
不要按"看起来无锁所以应该能进"推断 —— 表里的"未声明"就是没写过，既不算许可也不算禁止。
其中 `protocol/et_frame.h` 与 `protocol/et_atcmd.h` 两处头注**自身互相矛盾**（表内标"冲突"并照抄两文），
遇到这两个模块的 ISR 问题时以"补临界区"为安全侧，并留意 v3 候选里的收敛项。

Modbus 从站接入后，上位机侧可直接用工具验证（不依赖串口助手）：

```sh
python tools/modbus_master.py --selftest                 # 回环自测(不接串口)
python tools/modbus_master.py --port COM12 --read 0 --qty 4   # 真机读 4 个保持寄存器
```

---

## 排障三分法 (板上异常时)

1. **库侧**: 先在 host 跑 `make test` 复现——能复现就是库问题（补一条单测再修）;
2. **板侧**: 零警告交叉编译 + `AT+SELFTEST` 逐套件定位; 检查时钟/时基与 flash 几何三宏;
3. **接线/外设**: UART 波特率与地线、LED 极性、IWDG 窗口——三分法之外的问题多半在这里。

记录模板与真实案例见仓库根 `移植stm32实机记录.md`（G474 串口丢包、IWDG 复位循环、升级链走单）。
