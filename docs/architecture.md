# 架构总览 (Embedded_Tools v2.3)

> 本文回答"**是什么 / 怎么选**"；上手跑通见 [getting-started.md](getting-started.md)；
> 接口细节见 [API_GUIDE.md](API_GUIDE.md)；稳定性契约见 [API_STABILITY.md](API_STABILITY.md)。

## 1. 分层模型

```
┌────────────────────────────────────────────────────────────────────┐
│  应用层 (examples/、用户固件)                                       │
├────────────────────────────────────────────────────────────────────┤
│  debug/   et_shell(交互壳)  et_log(日志)  et_assert(断言钩子)        │
│           et_selftest(板上自测, 20 套件)                            │
├────────────────────────────────────────────────────────────────────┤
│  drivers/ et_key(按键)  et_led(LED)  et_spwm(软 PWM)                │
│  storage/ et_kv(掉电参数)  et_bootctl(安全升级 A/B)                  │
├────────────────────────────────────────────────────────────────────┤
│  protocol/et_crc(校验)  et_bytes(字节序)  et_frame(帧)              │
│           et_atcmd(AT 命令)  et_xmodem/_tx(固件传输)                │
│  sys/     et_stimer  et_sched(任务调度+耗时统计)  et_event          │
│           et_softclock(日历)  et_wdt(看门狗)                        │
├────────────────────────────────────────────────────────────────────┤
│  core/    et_ringbuf  et_queue  et_mempool  et_list  et_map  et_smap│
│  algorithm/ et_filter  et_medfilt  et_pid  et_stats  et_hist  et_fsm│
├────────────────────────────────────────────────────────────────────┤
│  port/    port.h 契约 (唯一碰硬件的层): 时基/临界区/putc/flash/wdt   │
└────────────────────────────────────────────────────────────────────┘
依赖单向向下: core/algorithm ← sys/protocol ← storage/drivers/debug ← 应用
```

**不变式**（v2.0 起冻结契约的一部分）：零动态内存、多实例句柄化、core/algorithm 零硬件
依赖（禁 port.h）、ISR-safe 范围逐头文件显式标注、MINOR 只追加。

## 2. 两条典型数据流

**UART 交互/升级流**（ISR → 主循环，事件驱动）：

```
UART RX 中断 ──字节──> et_ringbuf ──主循环排空──> et_shell/et_atcmd ──命令──> 应用动作
                                                     │
                     AT+UPGRADE: et_xmodem(收) ──整块──> port_flash_write(B 槽)
                                 et_bootctl: stage → 重启 → attempt → confirm/rollback
 ISR 置位: et_event (通知主循环"有包/有命令", 代替信号量)
```

**测量→控制流**（固定周期，前后台）：

```
ADC/传感器 ──> et_medfilt(去尖峰) ──> et_lpf1(平滑) ──> et_pid(控制)
                                                        │
                                             et_spwm ──> 执行器 ──> 被控对象 ──┐
                et_stats(判稳: 方差阈值)  <── pv 采样 <──────────────────────┘
                et_hist(分布: p99 长尾)   <── u/pv 采样
时基: et_sched 周期任务(et_sched_task_stats 看耗时) 或 et_stimer; 休眠配 et_sched_next_due+WFI
```

## 3. 模块选型表（按场景查）

| 你要… | 用 | 搭配/备注 |
|---|---|---|
| 中断与主循环传字节 | `et_ringbuf` | SPSC 无锁，ISR-safe 写 |
| 传"消息"而非字节流 | `et_queue` | 同款无锁技巧 |
| 固定内存块反复分配 | `et_mempool` | STRICT 防重复释放 |
| u32 键查表/资源索引 | `et_map` | 负载 ≤0.7，键 0 保留 |
| 字符串键配置表/命令路由 | `et_smap` | 大小写折叠用 `*_ci` |
| **消尖峰**（脉冲干扰） | `et_medfilt` | 前级；奇数窗 3~15 |
| 平滑抖动 | `et_filter`(movavg/lpf1) | 后级；medfilt→lpf1 两级 |
| 限制变化速率 | `et_filter`(slew) | PWM/设定值斜坡 |
| **闭环控制** | `et_pid` | 抗饱和=积分限幅；整定见 11.10 |
| 看均值/方差判稳 | `et_stats` | Welford 整数版，方差 Q10 |
| **看分布/长尾** | `et_hist` | p99 长尾视图，越界不丢 |
| 状态机/协议流程 | `et_fsm` | const 表驻 flash |
| 周期任务调度 | `et_sched` | 耗时统计 last/max（v2.2） |
| 单次/周期回调 | `et_stimer` | ISR 可启停 |
| ISR → 主循环通知 | `et_event` | 32 位标志 |
| ms tick → 日历时间 | `et_softclock` | UTC，1970–2106 |
| 防跑飞/阻塞段保护 | `et_wdt` | guard 包裹 flash 擦除 |
| **掉电参数保存** | `et_kv` | 双扇区乒乓+断电自愈；备份见 11.11 |
| **安全升级** | `et_bootctl` + `et_xmodem` | A/B 试运行/确认/回滚；流程例 ex_upgrade_flow |
| 帧协议解析 | `et_frame` | 头/长度/CRC 可配置 |
| AT 命令/交互壳 | `et_atcmd` / `et_shell` | Tab 补全 `ET_SHELL_TAB` |
| MCU 作发送方 | `et_xmodem_tx` | 与接收器共享常量 |
| 字段字节序打包 | `et_bytes` | 边界检查即唯一面 |
| CRC 校验 | `et_crc` | `ET_CRC_TABLE=1` 查表加速 |
| 日志/断言 | `et_log` / `et_assert` | 失败钩子可落 kv |
| 一条命令全模块冒烟 | `et_selftest` | 20 套件，板上可跑 |

## 4. 验证金字塔（质量门怎么咬合）

```
        板上自测 (et_selftest 20 套件, AT+SELFTEST)
       仿真回归  (Renode F103 smoke: kv/重启计数/selftest 20/20)
      host 单测  (409 用例 × 2 几何 + 1K 变体 + Tab 形态)
     配方载体    (make ex: 三例自检式示例, CI 常设)
    机制门       (docsync 210 断言 / apidump --diff 纯增 / sizecheck / docbuild)
```

层级关系：**下层红，上层必红**；示例（配方载体）用公开 API 编写——API 升级即编译错，
"可运行证据"不会静默腐化。

## 5. 设计边界（连续十一版的 Non-goals）

RTOS、动态内存、浮点格式化/浮点算法、i2c/spi 抽象、安全启动、多行编辑/通配、FOC/自动整定。
完整清单见各版计划的 Non-goals 章节与 [v3-candidates.md](v3-candidates.md)（破坏性候选唯一去向）。
