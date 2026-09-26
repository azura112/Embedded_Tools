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
| 库源 | `D:\code\My_Library\Embedded_Tools`整体拷入 `Core/et/`(v1.9.0 全量同步;v2.1.0 二次同步与复验见 §9,`diff -rq` 校验) |
| 板级接线 | LED=PC0(推挽,**高电平点亮已由走单 5 目视确认**)、按键=PD15(上拉,按下为低)、串口=PA9/PA10 外置 USB-TTL(CH343),115200-8-N-1 |
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

### 4.5 AT+SELFTEST 库化版板上记录 —— **17/17 PASS**(2026-09-08 v1.9.0 固件收口,v1.7 DoD)

v1.7 交付的 `debug/et_selftest` 库组件在 G474 真机正式跑通(取代 §4.4 私有版 13 套件;
xmodem 套件为 v1.8 升级版 **tx→rx 内存回环**,新增 atcmd/shell/kv/bootctl 覆盖):

| 套件 | 结果 | 套件 | 结果 |
|---|---|---|---|
| ringbuf / queue / mempool / list | PASS | filter / fsm / sched / event | PASS |
| stimer / crc / frame / softclock | PASS | wdt / atcmd | PASS |
| xmodem(tx→rx 回环) | PASS | kv / bootctl | SKIP(存储门控,默认裁剪) |

- 汇总行:`SELFTEST: 17/17 PASS`(15 PASS + 2 SKIP 计入),全程 **59ms**(17 套件);
- `AT+SELFSTOR`(破坏性存储套件实跑):`kv PASS` + `bootctl PASS` → `STORAGE SELFTEST PASS`,
  收尾 demo kv 自动重建(`kv: seq=1 …`、boot 计数归 1)——bootctl 套件含 v1.9 干净态修复(问题 8)。

### 4.6 tickless + RX 中断唤醒板面验证(v1.8 DoD,2026-09-08)

主循环为 tickless(next_due 门控 + WFI,RX 中断 = 唤醒源):深空闲 ≥20s(约 10 个心跳周期)
后敲 `AT+VER` **立即应答**(`ver=1.9.0 boot=1`),心跳/软时钟全程无丢拍——
RX 中断唤醒链路在 v1.9 固件上实证(问题 1 的修法经三版迭代最终板面闭环)。

## 5. 升级链真机走单 (v1.6, ★ 核心) —— 2026-09-08 收口执行

> 会话工具链(本版自动化): STM32CubeProgrammer CLI **v2.19.0**(ST-Link V2J47,烧录+`-Rst`) ×
> CH343 USB 串口 COM12 × Python 3.12 + pyserial 自研控制台(命令/复位窗口连续采集) ×
> `tools/pack_image.py` + `tools/xmodem_send.py`(走单 3 发送端)。
> 固件: v1.9.0(`0x10900`) 正式记录;走单暴露的 8 处缺陷(问题 3~10)修复全部由本版合入,全部由本版合入。

### 走单 1: SIMUPGRADE even → 超次回滚

```
ET> AT+SIMUPGRADE 2        → [at] sim image ver=2 written → STAGED slot B, rebooting
(复位 #1)                  → [boot] boot slot 1 attempt 1 → self-check FAILED (even ver), not confirmed
(复位 #2)                  → [boot] boot slot 1 attempt 2 → [boot] ROLLBACK: attempts exhausted
(复位 #3)                  → [boot] no staged slot
ET> AT+BOOTINFO            → staged=-1 confirmed=-1 attempts=0
```
| 项 | 结果 |
|---|---|
| attempt 计数递增 | ✅ attempt 1 → 2(两次复位日志逐行实证) |
| ROLLBACK 日志 | ✅ `ROLLBACK: attempts exhausted`(max_attempts=2,第 2 次失败即回滚) |
| 回滚后 boot 续走 | ✅ 第三次复位 `no staged slot`,BOOTINFO 全清零,旧镜像续走 |

### 走单 2: SIMUPGRADE odd → 自检通过 → confirm

```
ET> AT+SIMUPGRADE 3        → STAGED → (复位) → [boot] attempt 1 → [boot] slot 1 CONFIRMED (self-check ok)
(再复位)                   → [boot] boot slot 1 attempt 0   ← 已确认槽不再计数
ET> AT+BOOTINFO            → staged=1 confirmed=1 attempts=1 (恒定)
```
| 项 | 结果 |
|---|---|
| stage→confirm 迁移 | ✅ attempt 1 自检(奇数版本判据)通过 → CONFIRMED 日志 |
| 确认后 attempt 不累计 | ✅ 再复位 `attempt 0`、attempts 冻结于 1(staged 保留=已确认语义,见 et_bootctl.h 状态机) |

### 走单 3: AT+UPGRADE xmodem 真传输

```
ET> AT+UPGRADE                        → [at] send image via XMODEM-CRC (slot B)...
(主机) python tools/xmodem_send.py --port COM12 --file image.bin --timeout 60 --verbose
       → transfer done: 1932 bytes in 16 blocks (exit 0, CRC16 逐块 ACK, EOT 二段确认)
(板上) verify → abandon→STAGED → 复位 → [boot] attempt 1 → [boot] slot 1 CONFIRMED
```
| 项 | 结果 |
|---|---|
| 收包字节数/块数与发送端一致 | ✅ host `1932 bytes in 16 blocks` = 板端 `total=1932`(32B ETBI 头+1900B 体;镜像经 `tools/pack_image.py` 打包,ver=7 奇数) |
| 镜像 CRC 通过 → STAGED → 复位 | ✅ `et_bootctl_verify_image` 通过 → STAGED → 复位后自检 CONFIRMED |
| host 预验证 | ✅ 128B/1K 双块型端到端逐字节一致(2026-09-06,`--emit` + xmodem_host_recv) |
| 附注 | 槽容量=1 扇区(2KB),镜像上限 `slot_size−32`=2016B;`xm_sink` 本版加越界守卫(问题 6) |

### 走单 4: IWDG 真超时复位

```
ET> AT+WDTEST              → [at] IWDG armed 200ms, stop feeding -> reset expected (命令不复返)
(~200ms 后复位)
开机日志: [demo] reset cause: IWDG (watchdog timeout) + kv/boot 正常续走
```
| 项 | 结果 |
|---|---|
| 复位由 IWDG 触发(原因寄存器) | ✅ RCC IWDGRSTF 经 `reset cause: IWDG` 日志读出(判据先于 SFTRST 检查) |
| 复位后 boot#n+1 / 自检正常 | ✅ boot 计数 +1、kv/升级链照常(此前一次 WDTEST 后进入自愈喂狗,见问题 7/10) |
| 附加证据 | IWDG 启动后不可停:复位后 demo 检测 IWDGRSTF 自愈进入每循环 `et_wdt_feed()`,心跳连续(56s+ 稳定) |

### 走单 5: LED 极性 / 按键补录 —— ✅ 2026-09-09 上板人现场目视确认

| 项 | 结果 |
|---|---|
| PC0 极性实测(上电 3 连闪是否可见) | ✅ 上电 3 连闪(400ms 周期)清晰可见——**PC0 高电平点亮成立**,`spwm_led_write` 分支无需对调 |
| PD15 短按 → blink on / 长按 → breath on | ✅ 短按出 `demo: blink on` 慢闪;长按(≥600ms)出 `demo: breath on` 2s 呼吸,亮度渐变肉眼可见;"测试完全符合预期,正常运行"(上板人原话) |

> 本条为 v1.6 走单最后未闭合项;至此升级链走单 1~5 全部板上实证。**v1.6~v1.9 板面零欠账。**

## 6. 问题记录(实机暴露 → 定位 → 修复)

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

### 问题 3~6:升级链 demo 四连缺陷(v1.9 走单首轮暴露,demo 层)

| # | 缺陷 | 现象→根因→修复 |
|---|---|---|
| 3 | `et_bootctl_verify_image/stage` 误传**扇区号**(12) 而非**槽序号**(1) | 走单 1 首轮 `verify/stage failed`。API 首行 `slot > 1u` 拒绝 → 该路径自 v1.5 起在真机从未通过(Renode/CI 均未触发)。三 demo 统一 `BOOT_IDX_B=1u`,定义处注释两层"槽"语义(扇区号 vs 槽序号) |
| 4 | `SIMUPGRADE <ver>` 数字解析不归零(`ver=1u` 起步) | `AT+SIMUPGRADE 2` 打出 `ver=12`(1*10+2)。奇偶语义侥幸未坏但属解析 bug;入环前 `ver=0u` |
| 5 | 升级前未 `abandon()` | 状态机明示"confirm 后 stage 拒绝(先 abandon)",demo 未做 → 走单 2 后走单 3 必败。stage 链路前置 `et_bootctl_abandon(&g_bc) &&` |
| 6 | `xm_sink` 无槽容量守卫 | 误传 32KB 大镜像时 off 越过扇区 12 → 直接踩邻接**状态扇区 13**(G4 双字重编程 PROGERR)。补 `off+len > PORT_FLASH_SECTOR_SIZE` 拒绝;镜像上限 = 槽容量(2016B) |

### 问题 7:IWDG port 冷启动时序错误(F103/G474/CubeMX 三 port,v1.9 走单 4 暴露)

| 阶段 | 内容 |
|---|---|
| 现象 | `AT+WDTEST(200ms)` 打印 `wdt enable rejected (< 2x ERASE_MS_MAX)`,但契约下限=80ms 不该拒;且拒绝后 ~2s 板子真发生 IWDG 复位(一次性) |
| 取证 | 寄存器现场:PR/RLR 保持复位值、LSI 未起振;拒绝路径 `et_wdt_enable`→`port_wdt_enable` 返回 false 但**硬件已被启动** |
| 根因 | 旧时序 UNLOCK→PR/RLR→**等 PVU/RVU 清零**→START。软件模式下 **LSI 仅在 START(0xCCCC) 后起振**,PVU/RVU 是 LSI 时钟域同步标志——LSI 停振时永不清零 → guard(百万次)必然超时返回 false,而 START 又无条件写入 → 配置传输未完成狗已跑,首周期按缺省 RLR 超时(与 ~2s 观测吻合) |
| 修复 | 改参考 `HAL_IWDG_Init` 时序:UNLOCK→PR/RLR→**START(先起 LSI)**→等 PVU/RVU 清零→立即 `KR=FEED`(首周期完整)。三 port 同步修复;头注记录教训 |
| 教训 | 冷启动配置顺序是硅约定不是代码风格;`disable 不可逆`的硬件,失败路径必须保证"什么都没启动" |

### 问题 8:selftest bootctl 套件假设干净态(v1.9 SELFSTOR 首轮 6 断言连锁误报)

板上跑过真实升级(staged/confirmed 残留)后再跑 `AT+SELFSTOR`:初始态检查、stage、attempt、
confirm 连锁失败(6 checks)——套件隐含"从干净状态起步"却未执行。host 侧 flash 模拟器每跑全新,
永不暴露。修复:init 后强制 `et_bootctl_abandon()`(破坏性门控内合法且必要)。
**行号映射方法**:`check fail L849/876/880/882/883/884` 逐行回查 et_selftest.c,
一条根因解释全部 6 处(而非 6 个 bug)——断言行号 + 源码回查是板上失败定位的主路径。

### 问题 9:xmodem 会话成功判定用 `total`(DONE 时被清零)+ DONE 动作未回收尾 ACK(demo)

- `g_xm.total > 0u` 在 rx DONE 的 `session_reset` 后恒为 0 → STAGED 永不可达(与 v1.8 host 测试踩过的同一坑,demo 侧当时未修);改用**动作判定** `ok = (a == ET_XM_DONE)`;
- rx 对第二段 EOT 返回 `ET_XM_DONE` 动作,`xmodem_reply` 的 default 分支不回字节 → 发送端 `xmodem_send.py` 等 ACK 超时判失败。协议规定接收方对二段 EOT 回 ACK——DONE 分支补 `port_putc(ACK_BYTE)`;
- 附加防护:无对端时 rx tickless 静默重催块会让 demo 阻塞循环无限挂住(心跳消失,需复位救)——`cmd_upgrade` 加 60s 墙钟上限让出主循环。

### 问题 10:CubeMX demo WDTEST 复位后无自愈(v1.9 走单 4)

IWDG 启动后不可停(仅断电清除),而 demo 常规主循环不喂狗 → WDTEST 后每次复位 ~200ms 内必再复位(复位循环)。修复:开机复位原因检测 IWDGRSTF → `g_wdt_running=true` 进入每循环 `et_wdt_feed()` 自愈(1ms 主循环对 200ms 周期裕量充足),板子复位后恢复正常功能;`AT+WDTEST` 命令注释同步(不复返/自愈/仅断电清除)。

### 附:自测断言预期修正(host 预验证阶段发现,非库缺陷)

自测套件先在 host 预跑,修正 4 处断言预期:mempool 存储区需含位图/对齐开销;`et_lpf1` 首样本直通(primed)语义;`et_fsm` 扁平表**无源状态域**(同事件按表序首个 guard 通过者生效);list 遍历自删时被删节点不应计入访问序。修正后 host 11/11 PASS。

## 7. 挂账项(未验证,如实记录)

| 项 | 状态 |
|---|---|
| LED 点亮极性 / 按键短按长按 | ✅ 2026-09-09 走单 5 目视确认: PC0 高有效(3 连闪可见)、PD15 短按/长按行为全对——板面挂账清零 |
| `AT+SIMUPGRADE` / `AT+UPGRADE`(xmodem→bootctl 升级链) | ✅ v1.9 走单 1~3 收口(2026-09-08),暴露缺陷 3~6/9 已修 |
| IWDG 真超时复位 | ✅ v1.9 走单 4 收口:原因寄存器 IWDG + 自愈喂狗(暴露缺陷 7/10 已修) |
| 库内裸机移植轨(port/stm32g474) | 未单独上板(真机验证经 HAL 版 port,同契约) |
| USB(源工程 PCD 初始化) | 无应用语义,未移植 |
| Renode 仿真 | v1.7 政策关闭(不排期):G474 真机(AT+SELFTEST 17/17 + v1.6~v1.9 走单全套)已承担 G4 验证职责 |

## 8. 复现命令汇总

```sh
# 构建(零警告门)
cd D:\code\STM32CubeMX\G474VET6_ET_TEST
cmake --preset Release && cmake --build build/Release --clean-first
arm-none-eabi-objcopy -O binary build/Release/G474VET6_ET_TEST.elf build/Release/G474VET6_ET_TEST.bin
# 烧录: STM32CubeProgrammer @0x08000000,串口 PA9/PA10 115200-8-N-1
# 验证: AT+VER → AT+SELFTEST (期望 13/13 PASS) → 断电重启看 boot #n 递增

# ==== v1.9 走单收口会话 (2026-09-08, 自动化三件套) ====
# 烧录+复位: STM32_Programmer_CLI -c port=SWD -w build/Release/*.elf -v -rst
#                                    / -c port=SWD -Rst
# 打包镜像(ETBI 头+体, ver 奇=自检过):  python tools/pack_image.py --in body.bin --ver 7 --out image.bin
# 走单3 传输:                          (板上)AT+UPGRADE → (主机)python tools/xmodem_send.py #                                        --port COM12 --file image.bin --timeout 60 --verbose
# 走单1/2/4 + SELFTEST/SELFSTOR:       pyserial 控制台逐命令采集(见 board_final.log 样式)
# 期望: SELFTEST 17/17 PASS (59ms); SELFSTOR kv+bootctl PASS; WDTEST 后 reset cause: IWDG + 自愈
```

## 9. v2.1.0 板侧同步与复验 (2026-09-11)

**同步**:`Core/et/` 全量重拷 `core/algorithm/sys/protocol/drivers/debug/storage` + `et_config.h` + `port/port.h`(`et_port/`、`et_demo.c` 工程私有不动);`diff -rq` 七目录 + 三文件全 OK(v1.9 → v2.1 跨两版)。
**构建**:`cmake --preset Release && cmake --build --preset Release` → **0 warning**;`arm-none-eabi-size` = text **32032** / data **68** / bss **4560**,FLASH 占用 **32100 B**。
**零足迹事实(板侧零回归的直接证据)**:新增 `et_pid/et_stats/et_bytes` 未被 demo 调用,链接器把三者共 **59 个 section 全部丢弃**(map "Discarded input sections");FLASH 32100 B 与 v1.9 记录**逐字节等值** —— 同步不改变板端行为,仅版本串不同。

**板上复验**(烧录 `build/Release/G474VET6_ET_TEST.elf` + COM12 115200-8-N-1 采集):

```
[0][I][demo] Embedded_Tools v2.1.0 (0x20100)          # 版本宏板面自证
[57][I][demo] kv: seq=32 free=464 rec=98 key=2        # v1.9 起持久化数据完好
AT+VER        -> ver=2.1.0 boot=14
AT+SELFTEST   -> 17 suites 全 PASS, "SELFTEST: 17/17 PASS" (59ms)
AT+SELFSTOR   -> kv PASS + bootctl PASS -> "STORAGE SELFTEST PASS"
AT+SIMUPGRADE 3 (奇=自检过) -> STAGED slot B -> 复位 -> "slot 1 CONFIRMED (self-check ok)"
AT+SIMUPGRADE 4 (偶=自检败) -> STAGED slot B -> 复位 -> "self-check FAILED (even ver), not confirmed"
                               -> 再复位 -> "boot slot 1 attempt 2" + "ROLLBACK: attempts exhausted"
                               -> BOOTINFO: staged=-1 confirmed=-1 attempts=0 (回干净态)
```

结论:**板侧回最新验证态** —— selftest 17/17、存储套件、升级链 confirm/rollback 双路径均在 v2.1.0 固件上再次实证,与 v1.9 收口记录一致。

```sh
# ==== v2.1.0 板侧同步复验会话 (2026-09-11) ====
cd D:\code\STM32CubeMX\G474VET6_ET_TEST
cmake --preset Release && cmake --build --preset Release          # 0 warning
STM32_Programmer_CLI -c port=SWD -w build/Release/G474VET6_ET_TEST.elf -rst
# COM12 115200 采集: AT+VER / AT+SELFTEST / AT+SELFSTOR / AT+SIMUPGRADE 3|4 / AT+BOOTINFO
```


## 10. v2.2.0 板侧同步与 PID 闭环走单 (2026-09-12)

**同步**:`Core/et/` 全量重拷(七目录 + `et_config.h` + `port/port.h`),`diff -rq` 校验全 OK;本次起 `et_demo.c` 新增 PID 闭环命令组(工程私有,见下)。
**构建**:0 warning;FLASH **36088 B**(v2.1 为 32100)——增量来自 ① pid/stats/bytes 被 demo 真实调用(链入) ② selftest 17→20 套件 ③ PID 命令组,属**预期激活**(v2.1 的"零足迹"在 v2.2 解除)。
**板上自测**:`AT+SELFTEST` → **20/20 PASS**(pid/stats/bytes 三套件首次上板);`AT+SELFSTOR` → kv PASS + bootctl PASS。
```
[8340][I][selftest] SELFTEST: 20/20 PASS        # pid/stats/bytes 三套件首次上板即 PASS
```

**PID 闭环走单(P2,模拟被控对象 y += k(u-y),10ms 节拍;期望值 = host 同款定点公式仿真预计算)**:

```
命令组: AT+PIDSET <sp> <kp> <ki> <kd> [k] [imax] / AT+PIDRUN <ms> / AT+PIDOUT
        (增益与对象增益均为 Q15, 32768=1.0; imax=积分权限, 缺省 500)

组 A 保守: PIDSET 500 32768 131072 0 13107        (kp=1.0 ki=4.0 kd=0 k=0.4)
  仿真预期 peak=496 ov=0% settle=1110ms
  板上实测 PIDRUN 2000 -> "peak=496 ov=0% settle=1110ms"   —— 与仿真逐值一致
  PIDOUT: n=200 mean=441 min=207 max=496 var_q10=4047553

组 B 激进: PIDSET 500 131072 262144 0 13107 1000  (kp=4.0 ki=8.0, imax=1000)
  仿真预期 peak=610 ov=22% settle=2000ms
  板上实测 -> "peak=610 ov=22% settle=2000ms"              —— 与仿真逐值一致
  PIDOUT: n=200 mean=473 min=384 max=610 var_q10=2944087

组 C 积分钳位对照: 同 B 增益, imax=50(积分权限钳死)
  仿真预期: 积分被钳在 ±50 -> 输出上限 ~410 -> 稳态误差 ~18%(钳位法语义实证)
  板上实测 -> "peak=420 ov=0% settle=2000ms"
  PIDOUT: n=200 mean=410 min=399 max=420 var_q10=4565     —— 稳态钳在 410, 全程未进 ±5% 带
```

**升级链回归**:`AT+SIMUPGRADE 3`(奇)→ CONFIRMED;`AT+SIMUPGRADE 4`(偶)→ self-check FAILED;复位 → attempt 2 → `ROLLBACK: attempts exhausted` → 回干净态——confirm/rollback 双路径与 v2.1 §9 一致。
**开机横幅**:`Embedded_Tools v2.2.0 (0x20200)`;kv/bootctl 持久化在固件替换后保持(boot #8→#9 递增)。

```sh
# ==== v2.2.0 板侧会话 (2026-09-12) ====
cd D:\code\STM32CubeMX\G474VET6_ET_TEST
cmake --preset Release && cmake --build --preset Release          # 0 warning
STM32_Programmer_CLI -c port=SWD -w build/Release/G474VET6_ET_TEST.elf -rst
# COM12 115200: AT+SELFTEST(20/20) / AT+PIDSET+PIDRUN+PIDOUT(三组) / AT+SELFSTOR / AT+SIMUPGRADE 3|4
```

## 11. v2.4.0 板侧同步与 Modbus RTU 从站真机走单 (2026-09-12)

**同步**:`Core/et/` 全量重拷(七目录 + `et_config.h` + `port/port.h`),`diff -rq` 全 OK —— 本次为**跨 v2.2→v2.4** 两版同步(含 v2.3 的 et_hist 与 v2.4 的 et_modbus)。
**构建**:0 warning;**FLASH 37428 B**(v2.2 基线 36088 → +1340B = et_hist + et_modbus + demo 挂载),RAM 5312B。
**既有回归不破**:`AT+SELFTEST` **20/20 PASS**;`AT+SELFSTOR` kv+bootctl PASS;`AT+SIMUPGRADE 3` → CONFIRMED。

**挂载方案（偏差与理由，如实记录）**:计划预填 **USART2 独立**;bench 上只有一路 USB-TTL(CH343→COM12),故板侧实测采用 **USART1 帧首字节分流**:
```
分流规则: 突发首字节 == 从站地址 0x11 或 0x00(广播) → et_modbus; 否则 → et_shell
半帧延续: et_modbus_rx_pending() > 0 时后续字节一律继续喂从站
```
- **生产建议仍为 USART2 独立**（避免与 shell 争用、无分流歧义）；分流方案作为单串口场景的备选已文档化（API_GUIDE 5.7）。
- **分流局限（本记录的第 3 条已知限制）**:发往**其它从站地址**的帧（如 0x22）首字节不匹配 → 被分流到 shell 并回显（shell 语义），因此"地址不符静默"**不能**在共享口上验证 —— 该路径由 host 单测 `mb.addr_mismatch_silent` 与示例 `ex_modbus_slave` 覆盖。

**真机走单（`tools/modbus_master.py`，COM12 115200, 逐字节证据）**:

```
开机横幅 : Embedded_Tools v2.4.0 (0x20400)
          modbus rtu slave addr=0x11 (kv regs 1000..1003)
AT+VER   : ver=2.4.0 boot=5                       # shell 与 Modbus 共存

[c1] 0x03 读保持 0..3
  tx: 11 03 00 00 00 04 46 99
  rx: 11 03 08 00 01 00 02 00 03 00 04 59 D4       -> 1, 2, 3, 4
[c2] 0x04 读输入 0..3 (demo 同表)                  -> 1, 2, 3, 4
[c3] 0x06 单写 reg2 = 4660
  tx: 11 06 00 02 12 34 27 ED
  rx: 11 06 00 02 12 34 27 ED                      # 应答 = 请求回显
[c4] 0x10 多写 reg4..6 = 111,222,333
  tx: 11 10 00 04 00 03 06 00 6F 00 DE 01 4D EC 53
  rx: 11 10 00 04 00 03 C3 59
[c5] 回读 reg0..5: 1, 2, 4660, 4, 111, 222        # 写入生效
  tx: 11 03 00 00 00 06 C7 58
  rx: 11 03 0C 00 01 00 02 12 34 00 04 00 6F 00 DE 2A 73

异常三条:
  exc 0x02 (读 addr=100 越界)  tx 11 03 00 64 00 04 07 46  rx 11 83 02 C1 34
  exc 0x03 (qty=0)             tx 11 03 00 00 00 00 47 5A  rx 11 83 03 00 F4
  exc 0x01 (功能码 0x63, 静默路径界定)  tx 11 63 4D C9      rx 11 E3 01 A9 35
      ^ 与 host 示例 ex_modbus_slave 的期望字节**逐字节一致**(11 E3 01 A9 35)

静默路径(板侧):
  CRC 坏帧 (11 03 00 00 00 04 13 99) → <超时无应答>  # CRC 校验失败静默, 未误应答
  地址不符 (22 ...) → 分流到 shell(见上文局限), 非从站静默路径

kv 参数经 Modbus 读写的掉电验证:
  写 reg1000(kv key 10)=4660: tx 11 06 03 E8 12 34 06 5D → rx 回显
  软复位 → 读 reg5(RAM 演示寄存器) = 6              # RAM 复位回默认, 证明确已重启/读路径非陈旧 RAM
       → 读 reg1000(kv)          = 4660 (0x1234)     # flash 保持 ✓
  0x10 多写 kv 1001..1003 = 17,34,51 → 再复位 → 读回 17,34,51 ✓
  (真掉电语义由 kv 双扇区乒乓+逐条 CRC 设计与其掉电矩阵用例覆盖; 本处为软复位实证)
```

**过程自省（板侧暴露的两个问题，已修）**:
1. **静默路径应答未送出**:`et_modbus_tick()` 内产生的应答（未知功能码 0x63 的异常帧）在 demo 里没有 flush 调用点 —— `feed` 后 flush 覆盖不到 tick 路径 → 现象为"0x63 请求无任何应答"。修法: tick 后补 `mb_flush()`。**教训**: 应答可能由两条路径（feed 快路径 / tick 静默路径）产生, 调用侧两处都要发送。
2. **`et_log` 不支持宽度/补零修饰**: 初始化日志写成 `addr=0x%02x` → 输出字面量并错位消费后续参数（打印成 `addr=0x%02x (kv regs 17..1000)`）。et_log 头注只承诺 `%d/%i/%u/%x/%X/%c/%s/%p`, 已改回 `%x`。**教训**: 受限格式化器上别用 printf 的宽度/标志位。

```sh
# ==== v2.4.0 板侧会话 (2026-09-12) ====
cd D:\code\STM32CubeMX\G474VET6_ET_TEST
cmake --preset Release && cmake --build --preset Release              # 0 warning
STM32_Programmer_CLI -c port=SWD -w build/Release/G474VET6_ET_TEST.elf -rst
# 主站走单(COM12): python tools/modbus_master.py --port COM12 --read 0 --qty 4
#                   --write 2=4660 / --write-multi 4=111,222,333
#                   --read 100 --qty 4 --expect-exc 0x02 / --read 0 --qty 0 --expect-exc 0x03
#                   --raw-resp 11634DC9 --expect-exc 0x01 / --raw <bad-crc> (期望静默)
# 工具自测(不接串口): python tools/modbus_master.py --selftest
```

---

## 12. v2.5.0 板侧同步与**未执行的走单**(如实挂账) —— 2026-09-19 ｜ **已由 §13 承接**（历史占位节）

> **本节状态（v2.6-r2 G0′-5 / `CO-11(v2.6)` 标注）**：本节是 v2.5 交付期的**历史占位节**
> —— 当时板未接入，P2-2/P2-3/P2-4 未执行。板接入后这些走单已在 **§13**（v2.5 走单补齐）
> 与 **§14**（v2.6 板侧同步与 `CO-6` 验证）中全部执行完毕；**本节的"未执行"结论不再有效**，
> 引用证据请转向 §13/§14。保留原文仅为审计痕迹（不修改历史结论）。

> **本节结论先行**: 本版**只完成了库→板侧的同步与交叉编译**（P2-1），
> **P2-2 板侧主站命令组 / P2-3 真机走单 / P2-4 板上回归均未执行** ——
> 执行期 G474 板**未接入本机**（`Get-PnpDevice` 无 ST-Link；无 CH343 USB-TTL，
> 仅有 WCH-Link SERIAL(COM16)，属另一板系）。**不伪造任何板上结论**（HC-10 的
> 板上逐字节证据未取得）。处置依据 = v2.5 计划附 B 风险表"板档期"行：
> *"库内项（M1–M3）先合入；走单项如实挂账，不阻塞 M5 之外的进度"*。

**已完成：`Core/et/` 同步至 v2.5.0（跨 v2.4→v2.5）**

```sh
# 全量重拷七目录 + 两文件（et_port/、et_demo.c/h、main.c、CMakeLists USER CODE 区为工程私有, 不覆盖）
for d in core algorithm sys protocol drivers debug storage; do
  cp -f <repo>/$d/*.c <repo>/$d/*.h Core/et/$d/
done
cp -f <repo>/et_config.h Core/et/et_config.h
cp -f <repo>/port/port.h Core/et/port.h
diff -rq <repo>/{core,algorithm,sys,protocol,drivers,debug,storage} Core/et/{...}   # 七目录全 OK
diff -q  <repo>/et_config.h Core/et/et_config.h                                    # OK
diff -q  <repo>/port/port.h  Core/et/port.h                                        # OK
```

**构建**:`cmake --preset Release && cmake --build --preset Release` → **0 warning / 0 error**（37 个目标全部重建）。
**体积**:`FLASH 38712 B`（v2.4 基线 37428 → **+1284 B**）、`RAM 5312 B`（与 v2.4 等值）。
**增量归因（`arm-none-eabi-nm` 实测, 非推测）**:
- `et_modbus_master` 符号在 ELF 中**计数为 0** —— `--gc-sections` 丢弃未调用模块, **板侧零回归性质保持**
  （v2.1 建立的证明法仍成立）；
- +1284 B 来自**`et_log` 规格解析器加固**（`vformat` 0x4A0=1184B + `emit_number` 0x1AC=428B +
  两个 `emit_placeholder*` 与 `parse_spec` 内联）+ demo 侧日志格式串改为域宽形态。
  et_log 是板侧**实际使用**的模块，其体积增长属"功能换体积"的正当增量（缺陷清偿）。

**未执行项（挂账，附理由与触发条件）**

| 项 | 状态 | 理由 / 触发条件 |
|---|---|---|
| P2-2 板侧主站命令组 `AT+MBRD`/`AT+MBWR`/`AT+MBPOLL` | **未执行** | 该命令组必须改**共享 USART1 的分流/回显抑制**才能让主站收帧；而 v2.4 的分流规则是**板上验证过的**（首字节 ==0x11/0x00 → modbus，`et_modbus_rx_pending()` 判半帧延续）。**在无板可验的情况下改这条路径，风险是把已验证的从站路径改坏且无法察觉** —— 比留一个诚实的缺口更糟。触发条件 = 板接入本机 |
| P2-3 真机走单（逐字节：请求/应答/重发/异常/广播/日志行） | **未执行** | 同 P2-2（无板）；**HC-10 未满足**，见交付文档 §5/§6 |
| P2-4 板上既有回归（`AT+SELFTEST` / `AT+SELFSTOR` / `AT+SIMUPGRADE 3`） | **未执行** | 无板；smoke.sh 断言行本版零改动（selftest 维持 20，见下） |
| CO-2 / CO-11（USART2 独立口 / 共享口地址不符静默） | **维持暂缓** | 与 v2.4 同源硬件阻塞（仅一路 USB-TTL） |

**替代证据（本版已取得，非板上）**: 主站与日志两项的库内证据由
`test/test_modbus_master.c`(25 例) + `test/test_log.c`(24 例) + `make ex` 五例 +
`tools/modbus_master.py --selftest`(29 项，含**跨实现对拍**) 承担；
其中 `ex_modbus_master` 打印的请求/应答字节与 §11 的 v2.4 板上记录**逐字节一致**
（`11 03 00 00 00 04 46 99` / `11 06 00 02 12 34 27 ED` / `11 10 00 04 00 03 06 00 6F 00 DE 01 4D EC 53`
→ 应答 `11 10 00 04 00 03 C3 59` / 异常 `11 83 02 C1 34`）—— 这说明**协议组帧/解析实现与
板上已验证的从站侧同源**，但**不能替代**主站在真实串口时序（超时阈值、帧间静默、shell 争用）下的验证。

**下版（v2.6）板侧待办（按优先级）**:
1. 板接入后执行本节未执行的三项（P2-2/P2-3/P2-4），走单脚本已就绪：
   ```sh
   # 板侧主站 → PC 从站仿真器（不依赖第二台设备, 绕开 CO-2/CO-11）
   python tools/modbus_master.py --port COM12 --slave                     # 基础走单
   python tools/modbus_master.py --port COM12 --slave --slave-drop 2      # 超时重发
   python tools/modbus_master.py --port COM12 --slave --slave-exc 0x02    # 异常路径
   python tools/modbus_master.py --port COM12 --slave --slave-regs 1,2,3,4
   ```
2. `et_log` 域宽行板面原样比对（`%04u-%02u-%02u %02u:%02u:%02u`）。
3. selftest 套件 20 → 21（本版因无板复验而**维持 20**，见交付文档 §5）。

---

## 13. v2.5 板侧走单补齐（G0-1 返工轮，2026-09-24）

> 对应 `v2.5开发交付__…-r2.md` 的 AC-1 / HC-10：本节取得**板上逐字节证据**，
> 挂账的 P2-2/P2-3/P2-4 至此清零。

**环境**：

| 项 | 值 |
|---|---|
| 板 | STM32G474VET6（CubeMX 工程 `D:\code\STM32CubeMX\G474VET6_ET_TEST`，**库外私有**） |
| 调试/下载 | ST-Link SWD；`STM32_Programmer_CLI -c port=SWD -w build/Debug/G474VET6_ET_TEST.elf -v -rst` → `Download verified successfully` + `MCU Reset` |
| 串口 | COM12 / 115200（同一路即 v2.4 所用，仍**无**第二路 USB-TTL） |
| 固件来源 | 库 `main @ 82a0ae0`（v2.5 库内成果 + G0 文本修订，**不含 v2.6 代码改动**）；`Core/et` 全量重拷 + `diff -rq` 七目录 + `et_config.h` + `port.h` **全 OK** |
| 构建 | `cmake --build --preset Debug`，**0 warning**；FLASH **74488 B** / RAM **5960 B**（Debug 构建，与 Release 不可比） |

**板侧私有改动（库外，不在本仓 git）**：`Core/Src/et_demo.c` 增主站命令组
`AT+MBCFG <addr> [timeout_ms] [retry_max]` / `AT+MBRD <reg> <qty>` / `AT+MBWR <reg> <v0..v3>` / `AT+MBSTAT`；
事务期置 `g_mbm_busy`：**① 收字节一律喂主站**，② 暂停 v2.4 的从站分流（否则应答首字节 `0x11` 会被判为"发给板上从站的帧"），
③ shell 不参与（阻塞式，同 `AT+UPGRADE` 风格）。走单驱动 `build/board_walk.py`（**不入库**）：同一串口既做
PC 从站仿真（复用 `tools/modbus_master.py` 的 `LoopbackSlave` 内核，与 C 库语义对齐）又发 AT 命令并收集板侧日志。

### 13.1 主站正常读（请求/应答逐字节）

```
板: AT+MBCFG 17 100 2   → [at] MBCFG addr=17 timeout=100ms retry=2
板: AT+MBRD 0 4
req  11 03 00 00 00 04 46 99
resp 11 03 08 00 01 00 02 00 03 00 04 59 D4
板: [at] MBRD reg=0 qty=4 st=2 val=1 n=4 exc=0 t=32ms
板: [at] MBSTAT req=1 resp=1 exc_n=0 to=0 retry=0 crc=0 mismatch=0 late=0 disc=1
```
`st=2` = `ET_MB_OK`；读回值 1,2,3,4（与从站仿真器初值一致）；**请求字节 `11 03 00 00 00 04 46 99`
与 §11 的 v2.4 板上记录、与 `ex_modbus_master` 的 host 打印**逐字节一致。

### 13.2 主站写（0x06 回显）

```
板: AT+MBWR 0 4660          (0x1234)
req  11 06 00 00 12 34 86 2D
resp 11 06 00 00 12 34 86 2D
板: [at] MBWR reg=0 n=1 st=2 exc=0 t=30ms
```

### 13.3 丢包注入 → 超时重发（同一请求上线 3 次，字节逐次相同）

```
板: AT+MBRD 0 2            (PC 侧 drop=2: 前两个应答不发)
req  11 03 00 00 00 02 C6 9B     ← 第 1 次
req  11 03 00 00 00 02 C6 9B     ← 超时重发 1（超时 100ms）
req  11 03 00 00 00 02 C6 9B     ← 超时重发 2
resp 11 03 04 00 01 00 02 3B F3
板: [at] MBRD reg=0 qty=2 st=2 val=1 n=2 exc=0 t=236ms
板: [at] MBSTAT req=5 resp=3 exc_n=0 to=0 retry=2 crc=0 mismatch=0 late=0 disc=3
```
**重发字节与首帧逐字节相同（含 CRC）** ✓ 与 host 用例 `mbm.timeout_retry_exhaust` 的断言形态一致；
`retry=2`、`t=236ms`（≈ 2×100ms 超时 + 应答）符合 `resp_timeout_ms=100` 的换算。

### 13.4 异常注入（不重试，异常码上报）

```
板: AT+MBRD 0 2            (PC 侧 --slave-exc 0x02)
req  11 03 00 00 00 02 C6 9B
resp 11 83 02 C1 34
板: [at] MBRD reg=0 qty=2 st=3 val=0 n=0 exc=2 t=29ms
板: [at] MBSTAT ... exc_n=1 ...
```
`st=3` = `ET_MB_EXC`，`exc=2` = `ET_MODBUS_EXC_ILLEGAL_ADDR`；应答字节 `11 83 02 C1 34`
与 v2.4/v2.5 的跨实现向量**逐字节一致**；重发计数未增加（异常不重试 ✓）。

### 13.5 广播写（不等应答）

```
板: AT+MBCFG 0 100 2       → [at] MBCFG addr=0 ...
板: AT+MBWR 2 85           (0x0055)
req  00 06 00 02 00 55 E9 E4
(无应答帧)
板: [at] MBWR reg=2 n=1 st=2 exc=0 t=1ms     ← 立即终态 OK, 未进入等应答
```

### 13.6 `et_log` 域宽行板面原样（v2.5 加固的落点）

心跳行（每 2s）：`[2038][I][demo] alive 2038 ms | boot #2 | 2026-01-01 00:00:29 (UTC)`
—— 日期时间段的 `%04u-%02u-%02u %02u:%02u:%02u` **补零与域宽正确**（`01`、`00:00:29`），
即 v2.5 的"规格解析加固"在板面输出上成立（此前 v2.4 该段为 24 行手工补零绕行）。

### 13.7 板上既有回归

| 命令 | 结果 |
|---|---|
| `AT+SELFTEST` | `[selftest] start (20 suites)` → 18 PASS + kv/bootctl SKIP（存储门控）→ **`SELFTEST: 20/20 PASS`** / `[at] ALL PASS` |
| `AT+SELFSTOR` | `kv PASS` / `bootctl PASS` / `kv: seq=1 free=1976 rec=4 key=1` → **`STORAGE SELFTEST PASS`** |
| `AT+SIMUPGRADE 3` | `sim image ver=3 written` → `STAGED slot B, rebooting...` → 重启 → `boot slot 1 attempt 1` → **`slot 1 CONFIRMED (self-check ok)`**；横幅 `Embedded_Tools v2.5.0 (0x20500)` |
| 主站初始化 | `[demo] modbus rtu master peer=0x11 to=100ms retry=2` |

### 13.8 v2.4 从站分流路径复跑（证明板侧主站改动未破坏已验证路径）

PC 作主站（`tools/modbus_master.py --port COM12`），板作从站（v2.4 的分流规则不变）：

```
--read 0 --qty 4        tx 11 03 00 00 00 04 46 99  rx 11 03 08 00 01 00 02 00 03 00 04 59 D4  → 1,2,3,4 PASS
--write 0=4660          tx 11 06 00 00 12 34 86 2D  rx 11 06 00 00 12 34 86 2D                 → PASS
--read 0 --qty 2        rx 11 03 04 12 34 00 02 2E 85                                          → 4660, 2 PASS（写入生效）
--write 1000=1234       kv 参数通路                                                             → PASS
--read 1000 --qty 2     → 1234, 0                                                               → PASS
```

### 13.9 观察与遗留

- 统计里的 `disc`（丢弃计数）在正常读时为 **1**、累计到 3/4；其来源未在本次会话中定位
  （板上无单步跟踪手段，且不影响事务终态）—— **v2.6 的 `CO-6` 修复后复跑同一序列做对照**（见 §14）。
- 事务耗时 `t`：正常读 **32ms**、写 **30ms**、异常 **29ms**、广播 **1ms**、丢包重发 **236ms**
  —— 与 `resp_timeout_ms=100` 一致，说明**板上波特率/中断延迟下的超时换算取值可用**。
- 板侧新增命令组属**库外私有文件**（`Core/Src/et_demo.c`），不在本仓 git；库内 `examples/` 无对应改动。

---

## 14. v2.6 板侧同步与 `CO-6` 板上验证（2026-09-24）

**固件**：库 `main @ c642418`（v2.5 G0 文本 + 实机记录 §13 + v2.6 库内改动合流后）；
`Core/et` 全量重拷 + `diff -rq` 七目录 + `et_config.h` + `port.h` **全 OK**；
`cmake --build --preset Debug` **0 warning**，FLASH **75144 B** / RAM **5960 B**（较 v2.5 的 74488 B **+656 B**，
来自 `et_log` 修饰面（`emit_char_field` + `j/t/L` 分支）与 `et_modbus_master` 定长校验）；
`STM32_Programmer_CLI -c port=SWD -w build/Debug/G474VET6_ET_TEST.elf -v -rst` → 校验通过 + 复位。

### 14.1 版本与主站走单对照（v2.5 → v2.6，同一走单脚本 `build/board_walk.py`）

| 项 | v2.5（§13） | v2.6（本节） | 结论 |
|---|---|---|---|
| `AT+VER` | `ver=2.5.0 boot=20` | **`ver=2.6.0 boot=3`** | 版本升级生效 |
| 板上横幅 | `Embedded_Tools v2.5.0 (0x20500)` | **`Embedded_Tools v2.6.0 (0x20600)`** | ✓ |
| `MBRD 0 4` | `st=2 val=1 n=4 t=32ms` / `req=1 resp=1 crc=0 disc=1` | **逐值相同** | 无回归 |
| 请求/应答字节 | `11 03 00 00 00 04 46 99` / `11 03 08 00 01 00 02 00 03 00 04 59 D4` | **逐字节相同** | ✓ |
| `MBWR 0 4660` | `11 06 00 00 12 34 86 2D` 回显，`t=30ms` | 同 | ✓ |
| 丢包重发 | 3 次上线同字节，`t=236ms retry=2 disc=3 crc=0` | 同 | ✓ |
| 异常注入 | `11 83 02 C1 34`，`st=3 exc=2 t=29ms` | 同 | ✓ |
| 广播写 | `00 06 00 02 00 55 E9 E4`，无应答，`t=1ms` | 同 | ✓ |
| `AT+SELFTEST` | 20/20 | **20/20** | 维持 20（HC-12 显式声明） |
| `AT+SELFSTOR` | PASS | `kv PASS` / `bootctl PASS` / `STORAGE SELFTEST PASS` | ✓ |
| `AT+SIMUPGRADE 3` | `slot 1 CONFIRMED` | `STAGED slot B` → 重启 → **`slot 1 CONFIRMED (self-check ok)`** | ✓ |
| 从站分流复跑（PC 主站） | `1,2,3,4` PASS | `read 0..3 → 1,2,3,4` PASS、`read 1000..1001 → 1234, 0` PASS | ✓ |

### 14.2 `CO-6` 受害形态在板上**不复现**（关键结论）

`CO-6`（v2.5 评审探针发现）的受害前提是：**主站自身请求被自己的接收端读到**（半双工单线的回显），
噪声形如 `[addr][读功能码]` 使解析器按错误的字节数域定长。

本板 USART1 为 **TX/RX 独立**（USB-TTL 无回环），用"从站**完全不应答**"的对照实验验证：

```
AT+MBCFG 17 100 2 ; PC 侧 drop=99(不应答)
AT+MBRD 0 4
req  11 03 00 00 00 04 46 99     ← 第 1 次
req  11 03 00 00 00 04 46 99     ← 重发 1
req  11 03 00 00 00 04 46 99     ← 重发 2
[at] MBRD reg=0 qty=4 st=4 val=0 n=0 exc=0 t=302ms
[at] MBSTAT req=3 resp=0 exc_n=0 to=1 retry=2 crc=0 mismatch=0 late=0 disc=0
```
**`disc=0 / crc=0`** —— 板**没有**收到自己 TX 的字节（否则 v2.6 的新判据会把回显判为噪声并 `disc++`，
而 v2.5 的旧判据会 `crc_err++`）。据此：

- **`CO-6` 的修复在本硬件上属防御性修正**：受害形态（自身请求回显）在 G474 板上不成立，
  因此**无法取得"修复前/后"的板上对照**；其正确性由 host 侧 7 例（`mbm.noise_*`，含回显形态）承担。
- 真实单线半双工（如 RS-485 收发一体）场景才是 `CO-6` 的适用面 —— 该形态本板不具备，记为**已知边界**。

### 14.3 遗留观察

- 正常读/异常路径的统计里恒有 `disc=1`（v2.5 与 v2.6 **同值**，非 `CO-6` 引入）：来源未在板上定位
  （无单步跟踪手段），且**不影响任何事务终态**（`st=2/3` 均正确、值正确）。记 v2.7 排查项。
- 事务耗时与 v2.5 一致（读 32ms / 写 30ms / 异常 29ms / 广播 1ms / 重发 236ms / 超时 302ms），
  说明 `resp_timeout_ms=100` 的换算在 v2.6 下同样可用。

---

## 15. v2.7 板侧同批会话（从站解析对称化上板 + selftest 22 + `CO-8` 定位）—— 2026-09-25

> 对应 `v2.7开发计划__从站解析对称化与板上自测收口.md` §3.2 的 **P1-4 / P1-5** 与 AC-14/AC-15/AC-16（HC-9）。
> 环境：G474VET6 / **COM12**（CH343 USB-TTL）/ ST-Link，`STM32_Programmer_CLI -c port=SWD` 烧录校验通过 + 复位；
> 库 `HEAD = 724d089`（= `snap-2.7-start`，v2.6-r2 终态）与 v2.7 工作树分别出两个固件做 A/B。
> **落点声明（HC-8）**：本节全部改动在**板侧工程私有文件**（库外，不在本仓 git）——
> `D:/code/STM32CubeMX/G474VET6_ET_TEST/Core/Src/et_demo.c`（新增 `AT+MBSLAVE`/`AT+MBRAW` 两条诊断命令）、
> 同步目录 `...\Core\et\`；走单脚本 `build/board_c05_probe.py`、`build/board_disc_probe.py`（**不入库**，`build/` 已 gitignore）。

### 15.1 同步与构建（A = v2.6 库 / B = v2.7 库）

| 项 | A（`Core/et` ← `snap-2.7-start` = 724d089，**v2.6 库**） | B₁（`Core/et` ← v2.7 工作树） | B₂（B₁ + `AT+MBRAW` 探针） |
|---|---|---|---|
| 同步校验 | `diff -rq` 七目录 + `et_config.h` + `port.h` **全 OK** | 同 **全 OK** | 同 **全 OK** |
| 版本宏（板侧） | `2/6/0` | `2/7/0` | `2/7/0` |
| `et_modbus.c` 形态 | `expected_len` **旧形态**（`et_modbus.c:40` `return (n < 7u) ? 0u : (b[6]+9u)`；无 `head_may_be_frame`） | v2.7 形态（含 `head_may_be_frame` ×2 处） | 同 B₁ |
| `et_selftest.c` | v2.6（20 套件） | v2.7（含 `st_modbus`/`st_log`，22 套件） | 同 B₁ |
| `et_modbus_master.c` | v2.6 | v2.7（含 `ET_ASSERT((uint32_t)m->rx[2] == …)`，死代码已清） | 同 B₁ |
| `cmake --build --preset Debug` | **0 warning** | **0 warning** | **0 warning** |
| FLASH / RAM（Debug） | **75440 B / 5960 B** | **80852 B / 6744 B** | **81244 B / 6768 B** |
| `arm-none-eabi-size`（text/data/bss） | 75368 / 72 / 5888 | 80780 / 72 / 6672 | 81172 / 72 / 6696 |
| 用途 | `CO-5` 注入对照的**修复前**侧 | `CO-5` 修复后侧 + 完整走单（15.2） | `CO-8` 定位实验（15.4） |

> **体积增项归因（B₁ − A = +5412 B FLASH / +784 B RAM）**：① **RAM +784 B** = `et_selftest` 新增
> `modbus` 套件的**文件级静态缓冲**（主站双缓冲 2×`ET_MODBUS_ADU_MAX` = 512B + 从站 rx 32B/tx 64B +
> 少量标量）—— 与计划 P1-1 的"RAM 增项 ≥512B"预估一致；② FLASH 增项来自新增两个套件的代码
> （`st_modbus` 从站+主站段 + `st_log` 21 条断言）+ 从站解析边界改造 + demo 两条诊断命令。
> **发布配置 `ET_MODULE_SELFTEST=0` 不受影响**（自测代码整体裁剪）。

### 15.2 B₁ 板侧走单（P1-4：`AT+SELFTEST` 22 + 既有回归）

```text
### ver         → [I][at] ver=2.7.0 boot=7
### mbrd        → MBSTAT req=1 resp=1 exc_n=0 to=0 retry=0 crc=0 mismatch=0 late=0 disc=1 last_st=2
### mbwr        → MBWR 0 4660 → 回显 11 06 00 00 12 34 86 2D
### drop (丢 2 应答) → MBSTAT req=5 resp=3 … retry=2 … disc=3     (重发机制保持)
### exc  (强制 0x02) → MBSTAT req=6 resp=3 exc_n=1 …             (异常不重试)
### broadcast  → 广播写 00 06 00 02 00 55 E9 E4 无应答            (协议语义保持)
### selftest   → [I][selftest] SELFTEST: 22/22 PASS               ★ AC-14 关键证据
### selfstor   → [I][at] STORAGE SELFTEST PASS
### simupgrade → sim image ver=3 written → 重启 → [I][boot] slot 1 CONFIRMED (self-check ok)
```

**v2.4 从站分流路径复跑（PC 主站 → 板侧从站，`tools/modbus_master.py`）**：

```text
tx: 11 03 00 00 00 04 46 99 → rx: 11 03 08 00 01 00 02 00 03 00 04 59 D4   [modbus] read 0..3      : 1, 2, 3, 4  PASS
tx: 11 03 03 E8 00 02 46 EB → rx: 11 03 04 00 00 00 00 EB F2                [modbus] read 1000..1001: 0, 0        PASS
事后 AT+MBSLAVE → frames=2 resp=2 exc_n=0 crc=0 mismatch=0 disc=0 pend=0   （注：kv 已被 SELFSTOR 重置 → 1000..1001 读 0 属预期）
```

### 15.3 `CO-5` 板上注入对照（AC-15，同一注入形态 × 两版固件）

> **口径注记**（2026-09-26，`CO-13(v2.7)`，随 v2.8 G0-1）：本表 `AT+MBSLAVE` 读数（`crc`/`disc` 等）为**会话累计值**（自上电起累计，非单次注入增量），表内以绝对值呈现；**结论以 A/B 两固件同口径读数的增量对比为准**。

**注入形态**（评审探针 E10）：先 11 字节"伪写入帧"噪声（`FC=0x10` 且 `b[6]=4` 与 `qty=1` 不符 → 旧实现伪造帧长 13），
紧随真读请求；两种发送方式（`build/board_c05_probe.py`）：

- **form1** = 两笔写（11B ＋ 20 ms 间隔 ＋ 8B）—— 与评审探针的"两次 feed"同形
- **form2** = 一笔写（19B 全序列，单次 feed）
- 噪声 = `11 10 00 00 00 01 04 DE AD BE EF`；真请求 = `11 03 00 00 00 04 46 99`

| 项 | A（**v2.6 库**，修复前） | B₁（**v2.7 库**，修复后） |
|---|---|---|
| **form1**（两笔写） | 应答 `11 03 08 00 01 00 02 00 03 00 04 59 D4` ；`MBSLAVE frames=1 resp=1 crc=1 disc=0` | 应答 **同字节**；`MBSLAVE frames=1 resp=1 crc=0 disc=6` |
| **form2**（一笔写 19B） | **无 Modbus 应答**（只收到周期心跳日志行）；`MBSLAVE frames=1 resp=1 **crc=3** disc=0 pend=0` → **真请求被噪声吞掉**（frames 未增） | 应答 **`11 03 08 00 01 00 02 00 03 00 04 59 D4`** ✔；`MBSLAVE frames=**2** resp=2 **crc=0** disc=**17** pend=0` → 噪声逐字节重同步、真请求被正确解析 |
| `AT+RXSTAT`（全程） | `ore=0` | `ore=0` |

**结论（逐条）**：

1. **缺陷在板上可复现（A / form2）**：v2.6 从站对"`b[6]` 先于 CRC 被信任"的写入帧噪声整段前进 13 字节，
   把紧随的真读请求前 2 字节一并吞掉 → **无应答**、`crc_err` +2、`frames` 不增 ✔ 与评审 E10 逐项吻合。
2. **修复在板上生效（B / form2）**：同一注入形态下真请求**被正确应答**，`crc_err` 保持 **0**，
   噪声计入 `discarded`（`disc 0→17`，逐字节重同步）✔ 满足 AC-8/AC-15 的"应答有无 + `crc_err`/`discarded` 终值"对照。
3. **form1 在两版都"成功"——原因已定位且不影响结论**：板侧 `cfg.silence_ms = 4 ms`，两笔写之间的 20 ms 间隔
   足以让**静默路径**先处理掉 11 字节残帧（A：`crc_err +1`；B：`discarded`），于是真请求随后单独成帧。
   即"分两次发送到板上的形态"本就被静默路径兜住，**只有单次突发（form2）才暴露缺陷** —— 这正是
   `CO-5` 危害的实际触发条件（噪声与真请求落在**同一次突发**内），与 §14.2"主站侧受害形态在本板不复现"
   是同一类边界（该板的静默路径/独立 TX-RX 都在削弱半双工噪声场景），**不构成修复无效**。

### 15.4 `CO-8`：板上"正常读恒有 `disc=1`"的**物理来源已定位**（AC-16，B₂ 固件）

**机械证据（`AT+MBRAW`：打印**上次主站事务**收到的原始字节，事务开始时清零）**：

| 实验 | `MBRAW`（前 8 字节） | `MBSTAT` |
|---|---|---|
| A) 命令行 `AT+MBRD 0 4` **CRLF** 结束 | `n=14 first=`**`a`**` 11 3 8 0 1 0 2` | `req=1 resp=1 crc=0 … **disc=1** st=2` |
| B) 命令行 `AT+MBRD 0 4` **仅 CR**（无 LF 残留） | `n=13 first=11 3 8 0 1 0 2 0` | `req=1 resp=1 crc=0 … **disc=0** st=2` |
| C) 命令行 `AT+MBRD 0 4` **仅 LF** 结束 | `n=13 first=11 3 8 0 1 0 2 0` | `req=1 resp=1 crc=0 … **disc=0** st=2` |
| D) CRLF 命令行 ×3（复现性） | 3/3 均为 `n=14 first=`**`a`**` 11 3 8 0 1 0 2` | 3/3 均为 `disc=1` |
| E) `AT+RXSTAT` | `rx=372 ore=0`；`last=41 54 d a 52 58 53 54`（= `AT\r\nRXST`） | — |

**结论**：`disc=1` 的来源 = **命令行的 `\n`（0x0A）残留**：

- `AT+MBRD …\r\n` 中 shell 在 **`\r`** 处完成行分发，残余的 **`\n` 留在 USART1 RX 环缓冲**（实验 E 的 `last` 字节现场直接显示 `\r\n` 成对进入 RX）；
- `cmd_mbrd()` 随即启动**阻塞式主站事务**（`et_demo.c` 的 `mbm_run`），该循环把环内字节**逐字节**喂给主站 ⇒
  该 `\n` 成为本次事务收到的第 1 个字节（`MBRAW first=a`），与在途事务不同形 ⇒ `discarded++`、`crc=0`，终态 `st=2`（事务本身成功）；
- 用**仅 CR** 或**仅 LF** 结束命令行时（实验 B/C）无残留字节 ⇒ **`disc=0`** 恒成立；实验 D 的 3/3 复现排除随机性。

**已排除的假设（每条都有读数）**：

1. **UART 溢出/帧错误/噪声错误**：RX 中断里 ORE/FE/NE 计数器**全程 0**（`rx=372 ore=0`），且无 FE/NE 记录 ⇒ 不是 UART 层错误；
2. **板自身 TX 回环（半双工回显）**：`MBRAW` 首字节恒为 **0x0A** 而非本站请求/应答的任何前缀；§14.2 的"从站完全不应答"对照实验（`disc=0 / crc=0`）已独立证明本板无回环；
3. **物理层线路噪声**：若为线路噪声，取值应随机；实测**恒为 0x0A**（3/3）且与命令行结束符强相关 ⇒ 非物理层。

**性质与处置**：该字节来自**板侧 demo 的"shell 行解析 ↔ 阻塞式主站事务"接口残留**（`Core/Src/et_demo.c`，**库外私有代码**），
**不是库缺陷** —— 库在该形态下行为正确（与在途事务不同形的首字节 → `discarded`，不污染 `crc_err`，不影响终态）。
**修法（板侧一行级，本次不做以保留证据原样）**：进入 `mbm_run` 前先清空 `g_rx_rb` 中命令行残余（或让 shell 一并消费 `\r\n`）⇒ `disc` 归 0。
**记 v2.8 板侧待办**（库外，不影响库内门与发布）—— 已在交付文档 §5.2/§6 正式登记（`CO-8` 关闭）。

### 15.5 本节遗留

- `CO-11(v2.5)`（USART2 独立口 / 共享口地址不符静默的板侧验证）**再次暂缓**：bench 仅一路 USB-TTL，触发条件未变。
- `form1` 的"静默路径先兜住"现象提示：**单次突发**才是从站解析边界的真实压力形态 —— 已由 `test/test_modbus.c`
  的 `mb.sticky_bad_then_good`（单次 feed 内"坏帧+真帧"）固定为回归用例。
