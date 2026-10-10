# BLOCKED-2：v2.28 P2-3（REQ-7）真固件 IAP 双单受阻——板侧 B 槽几何容不下当版真固件

> 触发计划：`versions/v2.28/PLAN.md`（已批准-冻结）· P2-3 / AC-8 / HC-10③④⑤ · 提出者：执行者（B1，`prompts/B1.md`）· 状态：**受阻在案，待裁决**（本版其余批次不受阻，继续执行）
> 依据：计划 **HC-10⑤**「板侧构建/烧录出现红 = 真实发现，走 BLOCKED，不改库面绕过」；`PROCESS.md` 铁律 7（阻断停手，不硬扛不绕道）；`docs/开发计划模板.md`「判据自洽性检查」条（判据 vs 任务/实物冲突 = 计划缺陷，须申报而非凑数达成）。
> 性质：**判据物理不可达**（非操作失误、非库缺陷、非板不可达）——板在位、链路通、读数真，唯一缺口 = 镜像体量与槽容量。

## 1. 现象（计划要求 vs 板侧实物，逐值 + 可复跑命令）

| # | 计划要求（P2-3 / AC-8 / HC-10③④） | 实测（2026-10-10，G474 在位） | 证据命令与落盘件 |
|---|---|---|---|
| 1 | 镜像 body = **当版真固件**（v2.28.0 板侧构建产物，不含下一版代码） | 达成：Release preset 产物 FLASH **48140 B**、0 warning；`objcopy` 出的 `g474.bin` = **48140 B**，串扫描**仅含 `2.28.0`**（开工前该件 = 2026-09-08 的 32064 B 旧产物，已重建替换） | `cd D:/code/STM32CubeMX/G474VET6_ET_TEST && cmake --build --preset Release`（`build/v2.28-evidence/release_build.log`）→ `arm-none-eabi-objcopy -O binary build/Release/G474VET6_ET_TEST.elf build/Release/g474.bin` → python 字节扫描（`ls -l` 时间戳 2026-10-10 20:12） |
| 2 | 两单只差 `pack_image.py --ver` 奇偶（同一 body） | 达成：`img_odd_v7.bin` / `img_even_v8.bin` 均 **48172 B** = 32 B ETBI 头 + 同一 48140 B body；头实测 `magic=ETBI img_size=48140 ver=7/8`；sha256 前缀 body `8e8b20b69ae0cbee`、odd `ad1bc11741c5df15`、even `c575d5c2cc7e3c4b` | `python D:/code/My_Library/Embedded_Tools/tools/pack_image.py --in build/Release/g474.bin --ver 7 --out build/v2.28-evidence/img_odd_v7.bin`（ver 8 同） |
| 3 | `xmodem_send.py` **真传输进 B 槽** → confirm 单 / 回滚单各一 | **未达成**：两单均在块 17 被板侧取消——`RuntimeError: receiver CAN at block 17`，`xmodem_send.py` rc=1，耗时各 1.4 s（128 B 块 × 16 = 2048 B 即上限） | `build/v2.28-evidence/iap_dual_run.log`（含两次 `>>> subprocess` 命令行、xm-err 原文、失败后 `AT+BOOTINFO`/`AT+VER` 尾读）；驱动 `iap.py` 同目录（库外私有件） |
| 4 | `AT+BOOTINFO` 三字段判据（`staged/confirmed/attempts`） | 三字段**可读且逐值稳定**：受阻前后均为 `staged=1 confirmed=1 attempts=1`，`AT+VER` 恒 `ver=2.28.0` ⇒ 失败的传输未污染 bootctl 状态机（首块已擦页但未 stage、未复位） | `build/v2.28-evidence/walk_output.txt`（`AT+BOOTINFO-pre` / `-post` 两条）与 `iap_dual_run.log` 尾读 |
| 5 | 计划 §7②/§7④ 的起草期可行性核验结论 | **漏了一维**：§7② 实测了 `pack_image.py:34 hdr[16:20]=ver`、`--ver` 与 `ET_VERSION_MINOR` 无关、`g474.bin` 在位；§7④ 核了「REQ-7 无新 API」——**均未核 B 槽容量 vs 真固件体量**。实物：`et_demo.c:85-86` `slot_size = PORT_FLASH_SECTOR_SIZE`、`et_demo.c:72-74` 槽 = 参数区扇区号 11/12、`et_demo.c:286` `xm_sink` 拒收 `off + len > PORT_FLASH_SECTOR_SIZE`（v1.9 走单教训条文）、`et_port.c:27-28` G474 单页 = **2048 B**、`CMakeLists.txt:73-75` `PORT_FLASH_SECTOR_COUNT=16` ⇒ 参数区共 **32 KB**（`port.h:49-50`：sector 属参数区、偏移相对参数区首字节） | 上述五处 `grep -n` 实测行号；48140 > 2048（单槽）且 48140 > 32768（参数区全体）⇒ 单页与整区皆不足 |

## 2. 已执行 / 未执行的位置（不硬扛、不凑数）

**已执行**（P2 其余面与本版其余批次全部照常）：P2-1 同步与构建（`diff_rq.txt` 九行全 OK + sha256 72 件双侧一致 + 0 warning + FLASH 86424/RAM 8048，v2.27 → v2.28 = **+20 B** 归因 P1-1 守卫）；HC-10② 烧前机检（集合含 `2.28.0`，PASS 后方烧）；烧录（`Download verified successfully` + `MCU Reset`）；AC-7 常规走单五项 + `AT+BOOTINFO` 前后基线；P3-1~P3-4 脚本与逐要素对照（实机记录 **§18.3**，13 行对照表）。

**未执行**（避免伪达成与避免损坏实物）：

- **不以 `AT+SIMUPGRADE <奇/偶>` 充当 REQ-7 双单**——REQ-7 原文动机即「合成镜像通过 ≠ 真实发布固件通过」（v1.8 假镜像为对照源），以 64 B 合成镜像的状态机绿来勾 AC-8 = 造出需下版勘误的失实声称（HC-8①同族）。
- 不改 `storage/et_bootctl.*` / `port/` 几何、不动板侧链接脚本、不扩参数区——本版 §1.3 Non-goals（库面六目录仅 `debug/et_metrics.c` + `test/`，零公开面新增）与 HC-10⑤（不改库面绕过）双重禁止。
- 不擅自把 AC-8 改写为「部分达成」或降格判据：AC-8 按 **未达成** 登记，载体 = 交付 §2 表 + §3 偏差 + 本节。

## 3. 建议（可选）：三条修正方向，任选其一即可解阻

**路径 A（板侧工程分区改造，v2.29 认领，推荐）**：板侧私有工程做真 A/B 分区——参数区外另划两块 ≥ 当版最大构建体量的槽区（当前 Debug 86424 B ⇒ 建议各 **128 KB**），`et_demo.c` 的 `BOOT_SLOT_A/B` 改指新槽、`xm_sink` 改为**按页推进**（`off % 2048 == 0` 时擦该页、写长截断至页余量），`slot_size` 传新容量；库侧仅 `et_bootctl_cfg_t.slot_sector/slot_size` 语义随之放宽（**属契约面**，须走 MINOR + `API_STABILITY` + 板侧同步三件）。代价 = 一次板侧工程改造 + 一次库面契约评估，非一版顺车可得。

**路径 B（本版降级判据，需 Owner 批准计划变更）**：AC-8 改为「**真固件传输可达性实测**（含 CAN 根因与容量算式）+ `BOOTINFO` 三字段在两单失败态下逐值稳定」——即把「双单绿灯」换成本件 §1 的实测读数集。通道 = `versions/v2.28/PLAN.md` 文末「变更记录」追加（铁律 2，需 Owner 批准），执行者不自裁。

**路径 C（撤条 / 转下一版）**：REQ-7 从本版撤出，转 v2.29（与路径 A 同批），本版 AC-8 记「不适用（触发条件 = 板侧具备双槽分区，未成立）」，并照 §1.1 行 16 的显式裁决形态登记，禁静默。

**附带提请（不属本前置，不阻塞本版）**：计划 §7② 的「可行性实测」应增一维「**镜像体量 vs 目标槽容量**」的核验句——本次冲突正是在该维度未被核验时暴露的；是否收进 `docs/开发计划模板.md` §7 清单第②项由 A2/Owner 判定（同族 = `CO-13(v2.27)` 的判据口径面，已转 v2.29 判定）。
