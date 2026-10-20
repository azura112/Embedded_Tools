# BLOCKED-2：v2.29 真固件 IAP 双单的**传输面**在约 11–12 KB 处失步，88 KB 量级当版真固件无法完成 xmodem 传输

> 触发计划：`versions/v2.29/PLAN.md` · 提出者：执行者（B1） · 状态：待裁决
> 涉及任务：P2-4（真固件双单走单，REQ-7 / M4-④）；判据面：AC-8 的「真固件双单证据在档」一条
> **本件不阻断 v2.29 其余批次**（P0/P1/P2-1/P2-2/P2-3/P3/P4 均已实测在档，见 §2），仅使 AC-8 的一条子判据未闭合。

## 1. 现象（计划声明 vs 仓库事实，逐项对照 + 证据命令与输出）

| # | 计划声明 | 仓库事实（实测） | 证据命令与输出 |
|---|---|---|---|
| 1 | HC-10②「镜像 body = **当版真固件**」；AC-8「真固件双单证据在档（confirm：`AT+BOOTINFO` 三字段 + `AT+VER`；回滚：A 槽存活 + `attempts` 递增）」 | 真固件体量已能**装进**槽（分区面达成），但**传不进去**：当版真固件打包镜像 87,872 B / 687 块（128 B 块型）在实测中于 **block ≈ 91（约 11.6 KB）** 处由接收端发 **CAN** 中止；中止前每块平均约 **3.8 次发送**（ACK 计数 346 / 实到块 91）⇒ NAK 重试率高、无法完成 88 KB 量级传输 | `python build/v2.29-evidence/rxstat_probe.py build/v2.29-evidence/img_odd.bin` ⇒ `已 ACK 块数 = 346 / 687 … 停止 = (91, 'CAN')`；另一次独立复跑 `python build/v2.29-evidence/blk_trace.py …` ⇒ `停止于 block 91`（两次同点，非随机噪声）；件：`build/v2.29-evidence/{rxstat_probe.txt,blk_trace.txt,xfer_capture.txt}` |
| 2 | 同上（体量维本身） | **达成**：真固件 body = 87,840 B（`arm-none-eabi-objcopy -O binary` 自 build/Debug ELF），打包后 87,872 B ≤ 槽容量 **131,072 B**（余 43,200 B）；链接脚本 FLASH 长度已收到与槽容量同值 ⇒ 超容在**链接期**即失败（护栏实测在位，构建 exit=0） | `arm-none-eabi-size build/Debug/G474VET6_ET_TEST.elf` ⇒ `text=87600 data=80 bss=7992`；`python tools/pack_image.py --in build/v2.29-evidence/fw.bin --out … --ver 3` ⇒ `packed 87840 B img -> 87872 B`；`AT+REGIONS` ⇒ `region[2] base=0x08020000 size=131072 page=2048` |
| 3 | 「分区表实测（A/B/状态/参数区地址与容量，来源 = 链接脚本/CFG 实读）」 | **达成**：板侧读回四条区域表（与工程 `et_port.h` 定义逐值同）：`param 0x08078000/32768/2048`、`slot_a 0x08000000/131072/2048`、`slot_b 0x08020000/131072/2048`、`state 0x08040000/2048/2048`，`regions=4` | `python build/v2.29-evidence/regions_probe.py …` ⇒ 原文四行 + `REGIONS 命中条数 = 4`；件 `build/v2.29-evidence/regions.txt` |
| 4 | 「`AT+BOOTINFO` 三字段在案」可否作为 **confirm 单**证据 | **不可**：烧录只写应用区（0x08000000..+128K），参数区状态扇区（页 253 = 扇区 13）**未被擦除** ⇒ 走单读到的 `staged=1 confirmed=1 attempts=1` 是**既有残留状态**，不证明本版新做一次 confirm。本件如实登记为"读数在档但归因不成立"，不据此判 AC-8 该项达成 | `AT+BOOTINFO` 原文见 `build/v2.29-evidence/regions.txt` 与 `walk_output.txt`；`STM32_Programmer_CLI … -w … .elf` 的写入范围见 `build/v2.29-evidence/flash.log` |
| 5 | 「不新增板上 selftest 套件（22/22 维持）」 | **达成**：`AT+SELFTEST` ⇒ `SELFTEST: 22/22 PASS`（真机，bootctl 套件仍走参数区旧位形 cfg，见 §3 归因） | `build/v2.29-evidence/walk_summary.md` 行「AT+SELFTEST / 22/22 PASS」 |
| 6 | P2-3「G474 私有工程分区（库外链接脚本与配置）+ `et_bootctl_cfg_t` 尾部追加承载槽基址」 | **达成**：`port/stm32g474/port_stm32g474.h` 几何宏（库内）+ 工程侧 `Core/et/et_port/{h,c}` 真分区实现 + demo `g_bcfg.slot_base={0x08000000,0x08020000}` 与 `slot_size=131072`；`make WERROR=1 test` 526 例 / `test-g4` / `test-1k` / `test-tab` / `make ex` 六配方 / `docbuild` 两板零警告 / `sizecheck` 3/0 全绿 | 提交 `3307a1b`(P2a) / `cc6d1b7`(P4-2a) / 本批；`sh tools/docsync.sh` 计数与 `apidump --diff` 新增 7 项修改删除 0 项在案 |
| 7 | 板侧窗口可达性（走单前置） | 走单窗口内 **COM12 两次被外部进程长期占用**（`Win32_Process` 归因 = PID 43224 `…\.workbuddy\…\python.exe main.py`，非本仓探针残留、Agent 未杀该进程）⇒ 依赖端口的后续腿次（合成镜像双单、真固件重传）中途不可继续 | `python tools/board_window.py --mode full` 首轮 rc=2（端口类，P3-2 新分类）；`build/p32_evidence.log` 用例 2（有界 6×0.8s 仍拒）|

## 2. 已停手的位置

**已完成并入库（不受本件影响）**：P0（LICENSE/支持矩阵/十分钟样例/五处条文/M4，`20e767a`）· P1（CMake 接入面 + 双源一致性断言 + CI 新 job，`057cd2e`）· P2-1/P2-2（port 区域契约 + host 实现 + bootctl 位形 + 区域单测 6 例，`3307a1b`）· P4-2(a)（MINOR 29 / `0x021D00` + 版本键控面回刷，`cc6d1b7`）· P3-2（RC-1 串口异常面，`d701c41`）· P3-1（并发契约表 35 行 + 逐名对账断言，`68dd7dc`）· P2-3 库内侧（分区几何宏归位 + 默认位形修正）。

**板侧已实测在档（库外证据目录 `G474VET6_ET_TEST\build\v2.29-evidence\`）**：同步/构建/烧前版本串机检/烧录 verify 四件（构建三次，每次 warning=0、`verified=True`）+ 五要素走单两件（`ver=2.29.0 boot=53` / `FEATURES … sw=32` / `22/22 PASS` / MBRD 三形态 `disc=0` / `METRICS 11/12 drop=0` / `rx=180 ore=0`）+ 分区表读回 + 三组传输诊断件。

**本件停手处 = P2-4 的"真固件双单"两条腿**：
- confirm 单：未完成（传输在 ~11.6 KB 中止，`et_bootctl_verify_image(B)` 从未在真分区位形下对**完整真固件**跑过）。
- 回滚单：未完成（同上；且 `attempts` 递增链需先有一次成功的 stage 才有起点）。
- **未动面**：库面未为本件做任何绕行改动（未改 `et_xmodem` 重试/超时参数、未改 `et_bootctl` 判据、未把镜像换成小块合成体来"凑绿"——HC-8①③ 与 HC-10⑤ 禁止）。`debug/`、`protocol/`、`storage/` 三处与本件相关的实现本批 diff 为空（除 P2a 已登记的 bootctl 面）。

## 3. 建议（可选）：修正方向

1. **块型与缓冲**：本工程板侧是 128 B 块 + `g_xmbuf[132]`。88 KB ⇒ 687 块，每块一次完整"写 flash + ACK"往返。改 `ET_XM_1K=1` + `g_xmbuf[1032]` ⇒ 86 块，每块失步概率与总临界区次数同比例下降（属**板侧 demo** 改动，不触库面）。
2. **传输期把日志面停下来**：demo 的 `alive` 心跳每 2 s 阻塞式 `port_putc`（115200 下一行约 8 ms 占用 TX，且日志与 xmodem 应答交织进同一 TX 通道）。实测抓包里可见 CAN 之后紧跟 `alive` 文本行——上位机把日志字节当协议字节读，是最可疑的失步来源之一。建议 `cmd_upgrade` 期间静默心跳（板侧 demo 改动）。
3. **波特率**：本机链路 115200。若 1/2 后仍失步，可试 57600/19200 排除电气面（改 demo 一处 BRR，非库面）。
4. **判据侧**：若 Owner 判定本版 AC-8 允许"分区面达成 + 双单转下版"，建议把 AC-8 的该子条转入 v2.30 认领（与本件 §1 行 1 的实测读数一并作为输入），并让 `docs/v3-candidates.md` 记一条「板侧 xmodem 大块传输可靠性」候选，触发 = 下版真固件双单窗口。
5. **归因纠正项**（无论选哪条都要做）：本件 §1 行 4 登记的「`AT+BOOTINFO` 三字段来自烧录后未被擦除的参数区残留」这一点，须在交付文档 AC-8 行内明确写"不构成新 confirm 证据"，防止后续版本沿用同形态读数伪证。
