# STM32 实机移植记录 —— Embedded_Tools on G474VET6

> 记录日期:2026-09-06 ｜ 库版本:v1.5.0(`ET_VERSION_STRING` = 0x10500)
> 目标板:**G474VET6_ET_TEST**(STM32G474VET6,512K Flash / 128K RAM,Cortex-M4)
> 结论:**真机验证通过**——13/13 自测套件全过,kv 掉电持久化、串口交互、全模块冒烟在板上成立。
> 过程中定位并修复 1 个真机暴露的固件缺陷(UART RX)与 1 个跨平台布局适配(G4 flash 双字编程),详见 §5。

---

## 1. 环境注记(量化声明的复现前提)

| 项 | 值 |
|---|---|
| 工具链 | GNU Tools for STM32 **13.3.rel1**(STM32CubeCLT 1.18.0),`arm-none-eabi-gcc --version` 复现 |
| 构建系统 | CMake(Ninja)+ CubeMX 生成工程,`cmake --preset Debug/Release` |
| 宿主 | Windows / Git Bash |
| 库源 | `D:\code\My_Library\Embedded_Tools`(v1.5 + G4 8B 槽适配)整体拷入 `Core/et/` |
| 板级接线 | LED=PC0(推挽,假定高电平点亮)、按键=PD15(上拉,按下为低)、串口=PA9/PA10 外置 USB-TTL,115200-8-N-1 |
| 时钟 | HSI16 ×PLL = 144MHz(CubeMX `.ioc` 时钟树,FLASH_LATENCY_4) |

## 2. 两条移植轨

| 轨 | 位置 | 说明 |
|---|---|---|
| A. 库内裸机移植 | `Embedded_Tools/port/stm32g474/` | 自包含寄存器级 port(时钟树/启动/链接脚本),**未单独上板**,作为平台适配的参考实现与文档载体 |
| B. CubeMX/HAL 集成 | `G474VET6_ET_TEST/Core/et/` + `et_port/` | **本记录的真机验证对象**:port.h 契约以 HAL 实现(HAL_GetTick / HAL_UART_Transmit / HAL_FLASH / IWDG 寄存器),时钟/引脚/串口由 CubeMX 生成代码负责 |

两轨实现同一 `port/port.h` 契约;flash/IWDG 算法同源。集成布局见 `Core/et/README.md`。

## 3. 构建记录(零警告门)

```
cmake --preset Debug  && cmake --build build/Debug  --clean-first   # 0 warning
cmake --preset Release && cmake --build build/Release --clean-first # 0 warning
arm-none-eabi-size build/Release/G474VET6_ET_TEST.elf
```

| 构建档 | text | data | bss | 备注 |
|---|---|---|---|---|
| Release(集成+demo) | 18204 | 68 | 3028 | 不含 AT+SELFTEST |
| Release(含自测) | 28776 | 68 | 3744 | 本次验证版本 |
| Debug(含自测) | 50076 | 68 | 3744 | `-O0 -g3` |

镜像 LMA 末尾 ≈ 0x08007000,距 flash 参数区起点 0x08078000(末端 32KB)余量充足。

## 4. 真机验证记录(逐项)

### 4.1 启动与运行时寄存器现场

```
[0][I][demo] Embedded_Tools v1.5.0 (0x10500)
[4][I][demo] rxcfg: CR1=d ISR=6000d0 MODER=abebffff AFRH=770
[56][I][demo] kv: seq=4 free=1008 rec=64 key=2
[60][I][boot] no staged slot
ET>
```

- `CR1=0xD` = UE|RE|TE(接收器使能);`AFRH=0x770` → PA9/PA10 均为 AF7——运行时寄存器与 CubeMX 配置一致;
- `ISR=0x6000d0`:TEACK/REACK 置位,TXE/TC/IDLE 正常。

### 4.2 et_kv 掉电持久化(8B 槽适配的真机证据)

- 重启计数跨上电递增:**boot #5 → #10**(含复位键与断电重启);
- 页面统计与 8B 槽布局精确吻合:boot#5 时 `free=1632 rec=25` → 已用 416B = 页头 16 + 25×16B 槽(u32 值);boot#10 时 `free=1008 rec=64` → 1040B = 16 + 64×16;
- 软时钟 UTC 秒每 30s 持久化,断电后时间接续(日志 `2026-01-01 03:34:xx` 随上电次数单调推进);
- **seq 1→4**:页满触发压实(搬迁+提交)在真机 flash 上完成。

### 4.3 串口交互(AT 命令)

`AT+VER` / `AT+BOOTINFO` / `AT+RXSTAT` / `AT+HELP` 全部完整回显并正确应答;整串批量发送(串口助手 UTF-8)无损。

### 4.4 AT+SELFTEST 全模块冒烟 —— **13/13 PASS**(用户上板确认)

| 套件 | 验证点 |
|---|---|
| ringbuf | 写读一致、索引回绕、peek/drop、满写截断 |
| queue | FIFO 序、满/空语义 |
| mempool | 耗尽/复用/双重释放拒绝/池外指针识别 |
| list | 双端序、中删、遍历中自删安全 |
| filter | 滑动均值收敛(100→75→50→25→0)、Q15 低通 k=1.0/0.5/0 |
| fsm | 迁移、guard 拒绝→事件忽略、自迁移、无匹配忽略、重复 init 防护 |
| sched | 真实时基周期任务计数(10ms/25ms)、注销冻结 |
| event | 置位/取走即清/掩码相交/clear |
| stimer | 单次仅触发一次、周期 ≈N 次、stop 停止 |
| crc | 标准向量 F4 / 4B37 / 29B1 / CBF43926 + 流式==一次性 |
| frame | 组帧→解析回环、噪声重同步、CRC 篡改判坏帧 |
| softclock | 2026-01-01 / 纪元 0 / 闰日 2000-02-29 往返一致 |
| wdt | 契约负样本(下限拒绝 + disable 语义),不启动真看门狗 |

复现:烧录后发送 `AT+SELFTEST`(约 0.5s,期间心跳因周期追赶语义后补)。

## 5. 问题记录(实机暴露 → 定位 → 修复)

### 问题 1:UART RX 批量发送全丢(真机缺陷,已修复)

| 阶段 | 内容 |
|---|---|
| 现象 v1 | 发送 `AT+HELP\r\n` **零回显零响应**;心跳正常(主循环存活) |
| 诊断 | ① shell 回显无条件 → 排除"软件吞字节";② 加 `rxcfg` 寄存器现场导出 → `CR1=0xD`/AF7 全对 → 排除配置;③ 加 `AT+RXSTAT` + RX 排空后出现**碎片回显 `AHELP`/`AEL`** → 字节在到达但大量丢失 |
| 根因 | 主循环每 1ms `WFI` 睡眠,而 115200 波特率字节间隔 87µs;**RDR 仅 1 字节深度**,睡眠窗口内后续字节全部溢出(ORE)丢弃。行尾 `\r\n` 是最后两字节几乎必丢 → 命令行永不完成 → 无响应 |
| 修复 | RX 改**中断驱动**:`USART1_IRQHandler` 清 ORE/FE/NE(ICR)+ 字节写入 `et_ringbuf`(库自带的 SPSC 无锁环形缓冲,ISR 写/主循环读正是其设计场景);NVIC 优先级 5;`CR1.RXNEIE`;主循环与 `AT+UPGRADE` 均改为从环取字节 |
| 教训 | ① WFI 空闲循环下 UART **轮询接收不可行**,必须中断+环形缓冲;② "寄存器现场导出 + 原始字节计数"是把"玄学不响应"变成一次定位的关键手段(已固化为 `AT+RXSTAT`) |

### 问题 2:G4 flash 双字单次编程 vs 库存储布局(移植期适配,host 侧修复)

G4 flash 以 64 位双字为编程粒度且**每双字只允许编程一次**(目标非全 1 → PROGERR),而 F1 半字粒度允许位 1→0 重复写:

- `et_kv` 原两阶段页提交(16B 页头 + 后补 4B 提交字)在 G4 上必然重编程 → 拆为两笔 8B 双字(DW0=magic+seq,DW1=state+crc 提交时一笔写入),弃页语义不变;
- 记录槽改 `ALIGN8(8+len)`(`KV_REC_SLOT`),payload 整双字分块 + ≤8B 余量补 0xFF;
- `et_bootctl` 状态头 12B → 16B(尾部保留区保持擦除态);
- host 侧 279 例全量回归 ALL PASS(含掉电矩阵),board 日志 `free/rec` 数值与 8B 槽布局吻合(§4.2)为该适配的真机证据。

### 附:自测断言预期修正(host 预验证阶段发现,非库缺陷)

自测套件先在 host 预跑,修正 4 处断言预期:mempool 存储区需含位图/对齐开销;`et_lpf1` 首样本直通(primed)语义;`et_fsm` 扁平表**无源状态域**(同事件按表序首个 guard 通过者生效);list 遍历自删时被删节点不应计入访问序。修正后 host 11/11 PASS。

## 6. 挂账项(未验证,如实记录)

| 项 | 状态 |
|---|---|
| LED 点亮极性 / 按键短按长按 | demo 已实现,未做正式记录(PC0 高电平点亮为假定,低亮板需对调 `spwm_led_write`) |
| `AT+SIMUPGRADE` / `AT+UPGRADE`(xmodem→bootctl 升级链) | 代码在位、交互命令可用,未做真机走单记录 |
| IWDG 真超时复位 | 自测仅契约负样本;真超时复位行为未实测 |
| 库内裸机移植轨(port/stm32g474) | 未单独上板(真机验证经 HAL 版 port,同契约) |
| USB(源工程 PCD 初始化) | 无应用语义,未移植 |
| Renode 仿真 | G4 模型未验证,沿用库内 F103 smoke 模式挂账 |

## 7. 复现命令汇总

```sh
# 构建(零警告门)
cd D:\code\STM32CubeMX\G474VET6_ET_TEST
cmake --preset Release && cmake --build build/Release --clean-first
arm-none-eabi-objcopy -O binary build/Release/G474VET6_ET_TEST.elf build/Release/G474VET6_ET_TEST.bin
# 烧录: STM32CubeProgrammer @0x08000000,串口 PA9/PA10 115200-8-N-1
# 验证: AT+VER → AT+SELFTEST (期望 13/13 PASS) → 断电重启看 boot #n 递增
```
