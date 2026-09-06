# port/stm32g474 —— STM32G474 平台适配

实现 `port/port.h` 全部契约（临界区 / 毫秒时基 / putc / flash 参数区 / 看门狗），目标芯片 **STM32G474VET6**（512K Flash / 128K RAM），Cortex-M4 内核。

平台来源：`D:\code\STM32CubeMX\G474VET6_ET_TEST`（CubeMX 工程 `Core/`），其时钟树与外设引脚规划原样保留，移植为本库自包含裸机实现（**不依赖 HAL**，寄存器位定义对照该工程自带 CMSIS 头 `stm32g474xx.h` 与 RM0440 逐一核实）。

## 文件清单

| 文件 | 说明 |
|---|---|
| `port_stm32g474.c/.h` | port.h 契约实现（flash 参数区驱动 + IWDG）+ 平台初始化 |
| `stm32g474_min.h` | 最小寄存器定义（RCC/PWR/GPIO/USART1/FLASH/IWDG），自包含不依赖 CMSIS |
| `startup_stm32g474.c` | 向量表 + `.data/.bss` 初始化，不依赖 libc crt0 |
| `stm32g474vet6.ld` | 链接脚本：512K Flash @0x08000000 / 128K RAM @0x20000000，末尾 32K 参数区 ASSERT 保护 |

## 实现要点

- **时钟**：按源工程 `.ioc` 时钟树 HSI16 /1 ×18 /2 = **144MHz**（FLASH_LATENCY_4，Range 1）；SysTick `HCLK/8`=18MHz，RELOAD=17999 → 精确 1ms；USART1 内核时钟 = PCLK2，BRR=1250 → **115200-8-N-1**（零误差）。PLL 失锁自动回退 HSI16 16MHz（BRR/SysTick 同步换算），串口/时基仍正确。源工程的 USB 外设（MX_USB_PCD_Init）无应用代码，未纳入本移植。
- **临界区**：PRIMASK 保存/恢复 + 嵌套计数，与 F103 移植完全一致（Cortex-M4 同语义）。
- **缓存策略**：`FLASH_ACR` 仅保留等待周期，预取/ICACHE/DCACHE 全关。G4 擦写后缓存可能残留旧数据（F1 无缓存无此问题），关闭后 et_kv 读回校验语义与 F103 完全一致；追求性能可自行开启并按 RM0440 §3.6.6 在擦写后做缓存刷新。
- **双 bank 前提**：flash 几何以 `OPTR.DBANK=1`（出厂默认，2KB 页）为前提，`port_stm32g474_init` 复位后校验，违反则经串口报错停机。

## flash 约束（RM0440 §3.6/§3.7，G4 与 F1 的关键差异）

| 项 | F103（PM0056） | **G474（RM0440）** |
|---|---|---|
| 页尺寸 | 1KB（中容量） | **2KB**（双 bank 模式） |
| 页寻址 | 无页号（擦取指所在页） | `CR.PNB[10:3]` bank 内页号 + `CR.BKER` 选 bank |
| 编程粒度 | 16 位半字 | **64 位双字**（连续两笔 32 位字写） |
| 重复编程 | 允许（位仅 1→0 即可） | **禁止**：目标双字非全 1 → `PROGERR` |

因此 **storage 两模块做了 8B 槽适配**（平台无关，F1/host 语义不变）：

- `et_kv`：页头拆为两笔 8B 双字（DW0=magic+seq 先写，DW1=state+crc 提交时一笔写入），两阶段提交语义不变；记录槽 = `ALIGN8(8+len)`（`KV_REC_SLOT`），payload 按整双字分块 + ≤8B 余量补 0xFF 单笔写入；
- `et_bootctl`：状态头 12B → **16B**（尾部 4B 保留区保持擦除态），记录自偏移 16 起 8B 对齐追加；
- port 层对"4B 对齐但跨半双字"的调用做伴随字全 1 校验后合并编程，违反（重编程已写双字）按契约以**短写**截断上报。

参数区 = 片内 flash **末端 32KB**（16×2KB 页，全局页 240..255 = bank2 页 112..127），链接脚本 `ASSERT(_sidata_end <= _param_area_base)` 兜底。页擦典型 ~22.1ms / 上限 24.6ms（数据手册 tERASE），`PORT_FLASH_ERASE_MS_MAX=40ms` 保守取值；双 bank 支持边执行（bank1 代码）边擦写（bank2 参数区）。

demo 占用参数区**扇区 14/15** 作 et_kv 双扇区乒乓、**11/12/13** 作 bootctl 槽/状态：开机重启计数 +1 写回，软时钟 UTC 秒每 30s 持久化。

## flash 几何（编译选项，三处保持一致）

`et_config.h` 默认几何为 F103（1KB×16），G4 构建必须用 `-D` 覆盖（port 层有 `#error` 守卫防漏配）：

```sh
-DPORT_FLASH_SECTOR_SIZE=2048 -DPORT_FLASH_SECTOR_COUNT=16 -DPORT_FLASH_ERASE_MS_MAX=40
```

`stm32g474vet6.ld` 的 `_param_area_base`（0x08078000）与该几何联动，改动时同步。

## 构建（arm-none-eabi-gcc）

```sh
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c99 -Wall -Wextra -pedantic -Os -g \
  -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/stm32g474 \
  -DPORT_FLASH_SECTOR_SIZE=2048 -DPORT_FLASH_SECTOR_COUNT=16 -DPORT_FLASH_ERASE_MS_MAX=40 \
  -T port/stm32g474/stm32g474vet6.ld -nostartfiles \
  core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c storage/*.c \
  port/stm32g474/port_stm32g474.c port/stm32g474/startup_stm32g474.c \
  examples/stm32g474_demo.c \
  -o build/stm32g474_demo.elf

arm-none-eabi-objcopy -O binary build/stm32g474_demo.elf build/stm32g474_demo.bin
```

实测：**`-Wall -Wextra -pedantic` 零警告**。体积测量环境：GNU Tools for STM32 **13.3.rel1**（STM32CubeCLT 1.18.0），Git Bash 下 `-Os -g` 构建，`arm-none-eabi-size build/stm32g474_demo.elf` 读数（跨编译器版本存在布局级 ±16B 差异，以本环境复现为准）：

| 版本 | text | data | bss | 备注 |
|---|---|---|---|---|
| v1.5 | 15836 | 28 | 596 | 与 F103 demo 同全栈（含 8B 槽适配后的 kv/bootctl） |
| v1.6 | 15944 | 28 | 596 | +tickless 增量 API（next_due×2，demo 未调用） |
| v1.7 | 15948 | 28 | 596 | 默认（et_selftest 裁剪）；**全启用 `-DET_MODULE_SELFTEST=1`: text 23868 / bss 2268**（17 套件全量, +7920/+1672）—— DoD 体积增量记录 |

## 烧录与运行

1. ST-Link 连接 G474VET6：`STM32CubeProgrammer` 下载 `build/stm32g474_demo.bin @0x08000000`（或 `st-flash write`）；
2. 串口接 PA9(TX)/PA10(RX)，115200-8-N-1 查看日志；
3. 按键 PD15 对地（内部上拉）：短按启动/停止 LED 慢闪，长按切换软件 PWM 呼吸灯（LED PC0，高电平点亮；板载低电平点亮时对调 demo 内 `spwm_led_write`）；
4. 交互命令（回车结束）：`AT+VER` / `AT+BOOTINFO` / `AT+SIMUPGRADE <ver>`（合成假镜像走试运行/回滚状态机）/ `AT+UPGRADE`（真 XMODEM-CRC 收固件）/ `AT+HELP`；
5. 重启计数：上电或按复位键，日志 `boot #n` 递增 —— et_kv 掉电持久化的最直观验证。

## 平台状态

| 项 | 状态 |
|---|---|
| host 回归 | ✅ 291 例 ALL PASS（含 kv/bootctl 8B 槽适配后的掉电矩阵） |
| ARM 编译 | ✅ 零警告（GNU Tools for STM32 13.3.rel1） |
| 板上自测 | ✅ 工程私有版 13/13（v1.6 记录）；v1.7 库化版（17 套件, AT+SELFTEST/SELFSTOR）待上板回填 |
| **真机实测** | ✅ **G474VET6 上板通过**（boot #n 跨上电递增、AT 交互、AT+SELFTEST 13/13）——经 CubeMX/HAL 集成版 port（同 port.h 契约,HAL_GetTick/HAL_FLASH 实现）完成,记录见 `D:\code\STM32CubeMX\G474VET6_ET_TEST\移植stm32实机记录.md`;**本目录裸机 port 本体未单独上板** |
| Renode 仿真 | **不排期**（v1.7 政策关闭：G474 真机已承担 G4 平台验证职责,见 `AT+SELFTEST` 记录;模式参照仍可循 `port/stm32f103/renode/`） |

## 与源 CubeMX 工程的对应关系

| 源工程（G474VET6_ET_TEST/Core） | 本移植 |
|---|---|
| `main.c` SystemClock_Config（HSI16/1×18/2=144MHz，LATENCY_4） | `port_stm32g474.c` `clock_init_144mhz()`（寄存器直配 + 失锁回退） |
| `gpio.c`（PC0 输出下拉 / PD15 下降沿事件上拉） | `port_stm32g474_init()`（PC0/PD15 同配置；事件改轮询，由 et_key 扫描） |
| `usart.c`（USART1 PA9/PA10 115200，PCLK2 内核时钟） | `port_stm32g474_init()`（BRR=1250@144MHz） |
| `usb.c`（USB PCD，无应用代码） | 未纳入（无应用语义可移植） |
| `stm32g4xx_it.c` / `startup_stm32g474xx.s` | `startup_stm32g474.c`（按需稀疏向量表） |
