# port/ch32x035 — CH32X035G8U6 PD 测试板移植(et 重构版固件)

WCH CH32X035G8U6(Qingke RISC-V2A, 48MHz HSI, 62K flash / 20K RAM)平台移植,
载体是一块自制 USB-PD Sink 测试板固件的 **et 重构版**:应用逻辑与既已上板验证的
V1.7 基线逐行为对齐,骨架层(调度/缓冲/按键/LED/日志/看门狗)改用本库组件。

> **验证状态(如实声明,v2.9 P1-3 口径核清)**:`python build.py` 于 MRS2 RISC-V 工具链编译通过,
> app/port/et 层 `-Wall -Wextra` 零警告。**实机运行史**:et 重构版固件已在真机启动运行,
> 两处移植缺陷经实机定位修复(2026-09-26: `98d3612` SysTick 缺 PFIC 门控 → 时基冻结/喂狗
> 失效/IWDG 周期复位;`6bafe21` SysTick 被 WCH `debug.c` 延时函数轮询独占 → 时基改 TIM1;
> 命令通道移至 COM16(UART RX)、CDC 退为纯数据上报,`405265e`)。**上板回归清单(下)未系统
> 执行** —— PD 协商各档/CDC 枚举/LCD/按键/IWDG 时序/LED 三态等尚未逐项按通过计,故
> **不按"已验证"整体结论计**;应用 V1.7 基线功能以工程本体为准(回归时两版可逐行比对)。
> 本 port **不入 CI 七 job / 不入 sizecheck / 不设体积表行**(MRS2 工具链非本机与 CI 常设,
> 构建形态为 `python build.py`;再评估触发 = MRS2 工具链门建立或该板固件进入功能交付窗口)。

## 来源与基线

> **应用规范源**:自 2026-09-26 起,该板固件工程本体
> (`D:\code\mounriver-studio-projects\CH32X035G8U`,V1.8)已就地切换为 et 版
> (et 模块 vendor 进工程 `et/` 目录)。本目录保留为**库侧参考移植**,应用文件
> 与工程本体同源同版;后续改动以工程本体为准,此处随版本同步。

- 应用基线:`D:\code\mounriver-studio-projects\CH32X035G8U` V1.7
  (git 仓库已建,重构前提交为基线;PD/LCD/背光/CDC/IWDG/自动扫档均板上验证通过)。
- `PD_Process.c`(PD 协议栈)、`lcd.c`、USB 设备栈(EVT 移植)原样复制,未改动
  —— 时序敏感代码不进重构面。
- 重构面:`main.c`(et_sched 骨架)、`usb_cdc.c`(et_ringbuf/et_queue)、
  `board.c/h`(et_key 电平采样接入 + et_led 输出回调)、IWDG 归 port 契约。

## 目录结构

```
port/ch32x035/
├── port.h 契约实现
│   ├── port_ch32x035.c/h    # SysTick 1ms 时基 / mstatus 嵌套临界区 /
│   │                        # USART1 阻塞 putc / IWDG 三件套(flash 未实现, KV=0)
│   └── (SysTick_Handler 亦在此: 同时累加 PD 栈的 8 位旧制计数器 Tim_Ms_Cnt)
├── app/                     # 重构版应用(源工程 User/ 的重构面 + 原样复制件)
├── Startup/ Core/ Debug/    # WCH 启动文件 / 核支持 / printf 重定向(原样复制)
├── Peripheral/              # CH32X035 标准外设库(原样复制)
├── Link.ld                  # 62K flash / 20K RAM(原样复制)
└── build.py                 # 命令行构建(镜像 MRS2 编译参数)
```

## 构建

```sh
python build.py    # 产物: build/CH32X035G8U.elf/.hex/.bin(文件名与源工程一致,
                   # MounRiver/wlink 烧录流程不变)
```

工具链路径沿用源工程 `build.py` 的 MRS2 安装位置(`riscv-none-embed-gcc`)。
本 port 未接入库根 Makefile(MRS 工程文件不在复制范围,命令行构建即全量)。

### et 裁剪(见 build.py `-D`)

| 开关 | 值 | 说明 |
|---|---|---|
| `ET_RINGBUF_POW2` | 1 | CDC TX/RX 环容量 512/256 均为 2 的幂 |
| `ET_MODULE_KV` / `ET_MODULE_BOOTCTL` | 0 | flash 三件套未实现;如需 et_kv 须先补 `port_flash_*` |

参与编译的 et 模块:`et_ringbuf` `et_queue` `et_sched` `et_wdt` `et_key` `et_led` `et_log`。

## port.h 契约要点

- **时基**:TIM1 更新中断,48MHz/48/1000 → 精确 1ms(与 V1.7 逐配置相同,
  含 NVIC 优先级 0/3);中断里同时替 PD 协议栈累加 8 位旧制计数器
  `Tim_Ms_Cnt`(EVT `USBPD_SNK` 以 8 位回绕增量计时,应用每轮主循环做单读
  快照取 `Tmr_Ms_Dlt`)。
  ⚠ **时基不可放 SysTick**(实机教训):WCH `debug.c` 的 `Delay_Us/Delay_Ms`
  以轮询方式独占 SysTick(每次调用重写 CMP/CTLR),时基放 SysTick 会被延时
  改频/互踩 → 定时器全乱 → PD 状态机跑飞复位。SysTick 完整留给延时函数。
- **临界区**:`csrr/csrw mstatus` + MIE/MPIE 屏蔽 + 嵌套计数(与 F103 port 的
  PRIMASK 方案同构)。
- **putc**:USART1 阻塞发送(波特率由应用 `USART_Printf_Init(115200)` 配置)。
- **看门狗**:IWDG,LSI 取 ~47kHz(`PORT_CH32X035_LSI_HZ` 可覆盖)。
  ⚠ **PSCR/RLDR 在 LSI 时钟域**:改写后必须先等 PVU/RVU 清零再 Enable,
  否则计数器从错误暂态值起跑造成秒级复位风暴 —— 本板 V1.7 实机教训,序列已
  固化在 `port_wdt_enable()`;应用须在 `main()` 首行无条件 `port_wdt_feed()`
  兜底"上电继承短超时"(IWDG 启动后不可停且跨复位仍运行)。

## 与 V1.7 的行为差异(重构面)

1. **状态 LED 闪烁修复**:V1.7 手写实现以 10ms 槽累加器 `ms10` 做闪烁相位,
   `ms10/10` 恒为 0,"协商中快闪/无源慢闪"两分支实际恒灭(仅 contract 常亮
   生效);et_led 版按代码注释的**意图**实现:contract=常亮、协商中=200ms
   周期 50%、无源=800ms 周期 50%。
2. **诊断行格式**:`printf` 类诊断(开机横幅/wdt armed/VBUS lost)改走
   `et_log`(仅 UART),带 `[ms][级别][标签]` 前缀;每秒一次的
   `wdt feed N` 打印移除。**UART+CDC 双路数据行不受影响**:
   `VBUS=` 报告、`SWEEP*` CSV、全部命令应答仍走 `Rep_Printf`,逐字节同 V1.7。
3. **毫秒增量取法**:`Tmr_Ms_Dlt` 由"关中断双读"改为单读快照(语义等价,少一个
   关中断窗口)。
4. 其余(PD 状态机、命令集、LCD 界面、VBUS 采集、断连判据、扫档节奏)与 V1.7 一致。

## 上板回归清单(未执行,按通过计前必做)

- [ ] PD 协商 5/9/12/15/20V,`n`/`1-8` 切档重请求,断源重连
- [ ] `a` 自动扫档 CSV(SWEEP 行格式比对)
- [ ] CDC 枚举(COM 口出现)、命令回显、`s` 状态转储
- [ ] LCD 状态屏 + 背光 `b` 四档 + 方向微调命令
- [ ] 按键 KEY0/1/2(消抖 30ms 手感)
- [ ] IWDG:上电 ~10s 稳定运行(无秒级复位风暴=PVU/RVU 序列正确)
- [ ] LED 三态(contract 常亮 / 协商快闪 / 无源慢闪 —— 本版为修复后首验)
- [ ] UART 波特率 115200 诊断输出(et_log 前缀格式)
