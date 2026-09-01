# port/stm32f103 —— STM32F103 平台适配

实现 `port/port.h` 全部契约（临界区 / 毫秒时基 / putc），目标芯片 **STM32F103C8T6（BluePill 类板）**，Cortex-M3 内核。

## 文件清单

| 文件 | 说明 |
|---|---|
| `port_stm32f103.c/.h` | port.h 契约实现 + 平台初始化（时钟/引脚/SysTick/串口） |
| `stm32f103_min.h` | 最小寄存器定义，自包含不依赖 CMSIS；已有设备头可整体替换 |
| `startup_stm32f103.c` | 向量表 + `.data/.bss` 初始化，不依赖 libc crt0 |
| `stm32f103c8t6.ld` | 链接脚本：64K Flash @0x08000000 / 20K RAM @0x20000000 |

## 实现要点

- **临界区**：PRIMASK 保存/恢复 + 嵌套计数。首次进入保存并 `cpsid i`，嵌套只计数，计数归零恢复进入前状态；临界区内中断被屏蔽，计数本身无并发风险。
- **时基**：SysTick `HCLK/8`（复位后 HSI 8MHz → 1MHz），RELOAD=999 精确 1ms；ISR 内仅累加 `g_tick_ms`，自然回绕由上层无符号减法消化。
- **串口**：USART1 PA9(TX)，PCLK2=8MHz，BRR=0x045 → **115200-8-N-1**（误差 +0.64%），轮询 TXE 阻塞发送。
- 时钟假设：**未开 PLL**，全程 HSI 8MHz。需要 72MHz 时自行补 RCC/PLL 配置（BRR 与 SysTick RELOAD 同步调整）。

## 构建（arm-none-eabi-gcc）

```sh
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=c99 -Wall -Wextra -pedantic -Os -g \
  -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Iport -Iport/stm32f103 \
  -T port/stm32f103/stm32f103c8t6.ld -nostartfiles \
  core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c \
  port/stm32f103/port_stm32f103.c port/stm32f103/startup_stm32f103.c \
  examples/stm32f103_demo.c \
  -o build/stm32f103_demo.elf

arm-none-eabi-objcopy -O binary build/stm32f103_demo.elf build/stm32f103_demo.bin
```

v1.1 实测（GNU Tools for STM32 13.3.rel1）：**`-Wall -Wextra -pedantic` 零警告**，
`text=7976 data=4 bss=216`（Flash 占用 12%，RAM 1.1%），demo 含全部 16 模块。

## 烧录与运行

1. ST-Link 连接 BluePill，`STM32CubeProgrammer` 或 `st-flash write build/stm32f103_demo.bin 0x08000000`；
2. USB 转串口接 PA9，`115200-8-N-1` 查看日志；
3. 按键接 PA0 对地（内部上拉）：短按启动/停止 LED 慢闪，长按切换软件 PWM 呼吸灯。

## 已验证平台清单（API_GUIDE 第 7 章同步维护）

| 平台 | 编译 | 仿真 | 真机实测 | 记录 |
|---|---|---|---|---|
| host (MinGW gcc 16.1 / CI ubuntu+windows) | ✅ | ✅（虚拟时基单测） | ✅ 150 用例 | v1.0 起 |
| STM32F103C8T6 (arm-none-eabi-gcc 13.3) | ✅ 零警告 | ⏳ 顺延 | ⏳ 顺延 | v1.1 |

> ⏳ 项按 v1.1 计划"降级方案"处理：代码与编译验证已交付，Renode/QEMU 仿真与真机实测待硬件/仿真环境到位后于 **v1.1.1 补录**。Renode 参考：`machine create "stm32"；machine LoadELF "build/stm32f103_demo.elf"`（外加 sysbus.usart1 主机端串口重定向即可看日志）。
