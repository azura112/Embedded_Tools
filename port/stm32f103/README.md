# port/stm32f103 —— STM32F103 平台适配

实现 `port/port.h` 全部契约（临界区 / 毫秒时基 / putc / flash 参数区），目标芯片 **STM32F103C8T6（BluePill 类板）**，Cortex-M3 内核。

## 文件清单

| 文件 | 说明 |
|---|---|
| `port_stm32f103.c/.h` | port.h 契约实现（含 flash 参数区驱动）+ 平台初始化 |
| `stm32f103_min.h` | 最小寄存器定义（含 FLASH 模块），自包含不依赖 CMSIS |
| `startup_stm32f103.c` | 向量表 + `.data/.bss` 初始化，不依赖 libc crt0 |
| `stm32f103c8t6.ld` | 链接脚本：64K Flash @0x08000000 / 20K RAM @0x20000000，末尾 16K 参数区 ASSERT 保护 |

## 实现要点

- **临界区**：PRIMASK 保存/恢复 + 嵌套计数。首次进入保存并 `cpsid i`，嵌套只计数，计数归零恢复进入前状态；临界区内中断被屏蔽，临界区本身无并发风险。
- **时基**：SysTick `HCLK/8`（复位后 HSI 8MHz → 1MHz），RELOAD=999 精确 1ms；ISR 内仅累加 `g_tick_ms`，自然回绕由上层无符号减法消化。
- **串口**：USART1 PA9(TX)，PCLK2=8MHz，BRR=0x045 → **115200-8-N-1**（误差 +0.64%），轮询 TXE 阻塞发送。
- 时钟假设：**未开 PLL**，全程 HSI 8MHz。需要 72MHz 时自行补 RCC/PLL 配置（BRR 与 SysTick RELOAD 同步调整）。

## flash 参数区（et_kv，PM0056 依据）

| 项 | 本移植实现 | PM0056 依据 |
|---|---|---|
| 解锁 | KEYR 按序写 KEY1=0x45670123 / KEY2=0xCDEF89AB | 3.3.4 解锁序列 |
| 页擦 | CR.PER=1 → CR.STRT=1 → 等 BSY=0 | 3.3.2 页擦除流程 |
| 编程 | 4B 对齐块拆两回 16 位半字编程 | 3.3.1 主存储区编程 |
| 故障 | BSY 超时 / PGERR/WRPRTERR 按短写截断上报 | 3.3.1/3.3.2 状态位 |

- **F1 没有页地址选择寄存器**：页擦目标 = STRT 置位时刻的**取指页**。因此参数区必须与代码/常量页分离 —— 本移植把参数区放在片内 flash 末尾 16KB（16×1KB 扇区，`et_config.h` 默认几何），链接脚本 `ASSERT(_sidata_end <= _param_area_base)` 兜底，镜像侵入参数区直接链接失败；
- **擦写期间同区取指停顿**：契约约定只允许在 🏠MAIN 上下文调用擦写，port 内部以临界区包住完整擦写序列（ISR 不可能中途切入）；单扇区擦除约 20ms（`PORT_FLASH_ERASE_MS_MAX`），喂狗/调度安排由调用方负责；
- 只允许 1→0 编程（调用方保证目标已擦除），硬件置 `PGERR` 时按"短写"截断上报 —— 与宿主机模拟器语义一致，et_kv 会把短写页面自动弃用；
- `port_flash_read` 任意偏移；`port_flash_write` 强制 4B 对齐（违反返回 0）；`port_flash_erase_sector` 校验扇区号。

demo 占用参数区**扇区 14/15**（0x0800F800 起）作 et_kv 双扇区乒乓：开机重启计数 +1 写回，软时钟 UTC 秒每 30s 持久化；开机日志打印 `kv: seq=… free=… rec=… key=…` 可直接核对。

## 构建（arm-none-eabi-gcc）

```sh
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=c99 -Wall -Wextra -pedantic -Os -g \
  -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/stm32f103 \
  -T port/stm32f103/stm32f103c8t6.ld -nostartfiles \
  core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c storage/et_kv.c \
  port/stm32f103/port_stm32f103.c port/stm32f103/startup_stm32f103.c \
  examples/stm32f103_demo.c \
  -o build/stm32f103_demo.elf

arm-none-eabi-objcopy -O binary build/stm32f103_demo.elf build/stm32f103_demo.bin
```

实测（GNU Tools for STM32 13.3.rel1）：**`-Wall -Wextra -pedantic` 零警告**。
v1.1：`text=7976 data=4 bss=216`；v1.2（+et_kv+软时钟+重启计数）：`text=10892 data=4 bss=276`（Flash 17%，RAM 1.4%）。

## 烧录与运行

1. ST-Link 连接 BluePill：`STM32CubeProgrammer Download build/stm32f103_demo.bin @0x08000000`，或 `st-flash write build/stm32f103_demo.bin 0x08000000`；
2. USB 转串口接 PA9，115200-8-N-1 查看日志；
3. 按键接 PA0 对地（内部上拉）：短按启动/停止 LED 慢闪，长按切换软件 PWM 呼吸灯；
4. 重启计数：上电或按复位键，日志 `boot #n` 递增 —— et_kv 掉电持久化的最直观验证。

## 已验证平台清单（API_GUIDE 第 7 章同步维护）

| 平台 | 编译 | 仿真 | 真机实测 | 记录 |
|---|---|---|---|---|
| host (MinGW gcc 16.1 / CI ubuntu+windows) | ✅ | ✅（虚拟时基单测） | ✅ 206 用例 | v1.0 起 |
| STM32F103C8T6 (arm-none-eabi-gcc 13.3) | ✅ 零警告 | ⏳ 顺延 | ⏳ 顺延 | v1.1/v1.2 |

> ⏳ 项按 v1.1/v1.2 计划"降级方案"处理：编译验证已交付，Renode/QEMU 仿真与真机实测待环境到位后补录。Renode 参考：`machine create "stm32"` + `machine LoadELF build/stm32f103_demo.elf`（`sysbus.usart1` 主机端串口重定向可看日志）。