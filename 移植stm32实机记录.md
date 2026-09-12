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
