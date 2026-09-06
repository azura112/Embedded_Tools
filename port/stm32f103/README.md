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

实测：**`-Wall -Wextra -pedantic` 零警告**。体积测量环境：GNU Tools for STM32 **13.3.rel1**（STM32CubeCLT 1.18.0），Git Bash 下 `-Os -g` 构建，`arm-none-eabi-size build/stm32f103_demo.elf` 读数（跨编译器版本存在布局级 ±16B 差异，以本环境复现为准）：

| 版本 | text | data | bss | 备注 |
|---|---|---|---|---|
| v1.1 | 7976 | 4 | 216 | 16 模块 |
| v1.2 | 10892 | 4 | 276 | +et_kv+et_softclock |
| v1.3 | 11208 | 4 | 276 | +et_fsm（demo 含 kv 重启计数） |
| v1.4 | 12276 | 4 | 276 |
| v1.5 | 15468 | 24 | 596 | +et_xmodem+et_shell；kv/bootctl 8B 槽适配（G474 双字约束）后复测 |
| v1.6 | 15576 | 24 | 596 | +tickless 增量 API（next_due×2，demo 未调用；-nostartfiles 无 gc-sections 全量入 ELF） |
| v1.7 | 15580 | 24 | 596 | +et_selftest 组件入库（默认裁剪，demo 未启用，仅版本宏级增量） |

## Renode 仿真（v1.3 起为 CI 常设门）

`renode/smoke.sh` 在 headless Renode 中跑通 demo 并断言串口关键日志（横幅 / `kv: seq=…` / UTC 心跳 / `boot #1` / **复位后 `boot #2`** —— 后者即 kv 掉电持久化的功能证据）：

```sh
arm-none-eabi-gcc ...(同上, 产出 build/stm32f103_demo.elf)
sh port/stm32f103/renode/smoke.sh <renode可执行> <renode安装根> <仓库根> <输出目录>
```

仿真要点（Renode 1.16.1，与真机差异按计划 §7 记录在案）：

| 项 | 处理 |
|---|---|
| F103 flash 控制器无寄存器模型 | `sysbus Tag <0x40022000,0x40022013> "FLASH" 0x0`（读 0=空闲/无错误），存储体可写；**只验功能不验擦写时序** |
| flash 上电态 | `flash_erased_64k.bin`（全 0xFF）先填充再 LoadELF，模拟真实擦除态 |
| 有界运行 / 复位 | `emulation RunFor`（不可先 `start`）；`sysbus.cpu Reset` 软复位（内存保持） |
| 本 smoke 曾揪出的 bug | v1.2 port_flash_write 块内偏移错误（固定 src[0..3]）——宿主机全绿未暴露，仿真首跑即现形 |

CI：`.github/workflows/ci.yml` 的 `renode-smoke` job（Renode 固定 1.16.1，失败保留 uart 日志 artifact）；`release.yml` 发布前强制过此门。

## 烧录与运行

1. ST-Link 连接 BluePill：`STM32CubeProgrammer Download build/stm32f103_demo.bin @0x08000000`，或 `st-flash write build/stm32f103_demo.bin 0x08000000`；
2. USB 转串口接 PA9，115200-8-N-1 查看日志；
3. 按键接 PA0 对地（内部上拉）：短按启动/停止 LED 慢闪，长按切换软件 PWM 呼吸灯；
4. 重启计数：上电或按复位键，日志 `boot #n` 递增 —— et_kv 掉电持久化的最直观验证。

## 已验证平台清单（API_GUIDE 第 7 章同步维护）

| 平台 | 编译 | 仿真 | 真机实测 | 记录 |
|---|---|---|---|---|
| host (MinGW gcc 16.1 / CI ubuntu+windows) | ✅ | ✅（虚拟 flash+时基单测） | ✅ 291 用例 | v1.0 起 |
| STM32F103C8T6 (arm-none-eabi-gcc 13.3) | ✅ 零警告 | ✅ Renode smoke（本机+CI 门） | 待硬件（常设挂账，**不阻塞发布**） | v1.1 编译 / v1.3 仿真闭环 |

> **v1.3 政策**：Renode CI 门作为 F103 的功能验收线（断言 kv/重启计数等日志）；真机记录转常设挂账，硬件到位后按 checklist 补录（重启计数 `boot #n` 递增即为最直观验收），不再随版本顺延阻塞。