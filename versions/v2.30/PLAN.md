# v2.30 开发计划：真固件升级链闭合与参数版本化（功能版）

> 填写：规划者（阶段 2，A1 对话 2026-10-21）。批准依据 = Owner standing 规则「当我让你规划下一个版本时，默认批准」→ 交付即冻结。冻结后执行者只打勾；变更只走文末变更记录（需 Owner 批准）。
> 布局：三件套沿 `versions/v2.30/`。权威模板 = `docs/开发计划模板.md`；评审侧 = `docs/评审记录模板.md`。
> 板侧工程与证据目录在**库外**（不在本仓 git），全路径见 §3 P1 与 §7③。

- 状态：**已批准-冻结**（§7 六项已过，文末合规声明在案）
- 判据档位：**全量档（功能版，出口 A）**
- 基线 commit：**`tag v2.29` = `810d355`**（本地实测在位且为 `main` 祖先）。**异常登记**：该 tag **未推送、Release 未生成**（Owner 2026-10-21 明示「不进行 push，继续下一版」；`git ls-remote --tags origin v2.29` 实测 **0 行**）⇒ 见 **HC-0** 的推送顺序约束与偏差申报
- 版本目标：v2.30.0（`ET_VERSION_MINOR` 29→30，hex `0x021E00`；实测现值 29 / `0x021D00`）
- 服务阶段目标：**M4-④ 收尾**（真固件双单）+ **M5-①**（M5「参数与故障可维护性」由 P0-5 登记）

- **价值声明（行为级）**：本版交付后——① **现场升级链真可用**：当版真固件（body **87,840 B**、打包 **87,872 B**、槽容量 **131,072 B**）能经现有串口链路完整传入 B 槽并走完 **confirm + 回滚** 两条腿（v2.29 实测在 128 B 块型下于约 **11.6 KB / block 91** 处被接收端 CAN 中止；根因锁定 = 687 块逐块往返 + demo 每 2 s 的 `alive` 心跳日志与 xmodem 应答共用同一 TX）；② **参数可随固件升级**：v1→v2 schema 自动迁移、迁移失败回滚旧配置、保存中断电 **100 次**不出现半套、恢复出厂保留设备身份/校准 key；③ **判定口径纠偏**：`AT+BOOTINFO` 的残留读数**不得**再被当作"本版新做一次 confirm"的证据（v2.29 `BLOCKED-2` §1 行 4 的归因纠正）。

## 1. 目标与范围

### 1.0 价值声明与立项判定声明

- **四路盘点（2026-10-21 判定对话实测）**：

| 路 | 结果 |
|---|---|
| ① 用户直连 | 指令 = 「不进行 push，继续下一版」；三项裁决 = 认领组合「升级链闭合 + N-03 参数版本化」/ ROADMAP「M4 收尾 + 新建 M5」/ 修复边界「**只板侧修，库面零改动**（若仍失步则升级为库面任务）」。另：v2.29 的 Release 由 Owner 保留未发（不 push），本版以本地 tag 起线 |
| ② BACKLOG.md | **REQ-7 = 部分完成，本版认领闭合**（v2.29 已落 A/B 分区与 `port` 区域契约；剩传输面 + 双单，来源 = `BLOCKED-2`；`CO-6(v2.29)-P` 裁决 = 认领 v2.30）。**REQ-12（N-03 参数版本化，P1）→ 纳入**。REQ-11（FreeRTOS port 本体）/ REQ-13（黑匣子）/ REQ-14（断点续传+签名）/ REQ-15/16/17 → **暂缓**（触发 = 后续版本认领）。REQ-2/3/4 触发未现（v2.29 板侧窗口实测 COM16/WCH-Link 不在位）；REQ-10 / REQ-18 = 已完成（v2.29）。速记区空 |
| ③ 最近一版评审记录 CO 存量 | 输入源 = `versions/v2.29/REVIEW.md` §5（r1，CO-1…CO-6）+ `versions/v2.29/REVIEW-r2.md` §5（r2，CO-7/CO-8/CO-9；**复评结论 = 通过**，0 阻塞/0 返工）。r1 的 CO-1…CO-5 已由 r2 清偿并经复评；**CO-6(v2.29)-P = 维持未闭合 → 本版认领**（行 6）。r2 新增三条**均低**：CO-7（r2 档"末笔"表述残留两处）/ CO-8（§4 围栏计数失准）/ CO-9（tag 后收尾笔使 docref (d) 面退回未登记；token 已于 `980210a` 统一登记，**条文半边待本版**）。另：**`BLOCKED-2` 五条建议**（1K 块型 / 心跳静默 / 波特率兜底 / 判据转版 / BOOTINFO 归因纠正）为本版 P1 的直接输入。跨版承接：`CO-12(v2.28)`（门区间右端 `--from`）/ `CO-9①(v2.27)`（[自指] fail/诊断行）维持 v3 池；`CO-8(v2.28)-P` 已由 clang 路闭合 |
| ④ ROADMAP.md 未达判据 | **M4-①②③ 达成**（v2.29 复评通过）；**M4-④ 部分**——真 A/B 分区与 `port` 区域契约落地，**真固件双单传输面受阻** ⇒ 本版收尾（P1）。M3 已于 v2.29 判定结案。M1 已结案（②③④ 转常挂账）。M2（v3）触发未现。**M5 新建**（P0-5）承接 N-03/N-04 方向 |

- **三出口与档位声明**：**出口 A 功能版**——判据 = ≥1 项用户可感差异任务（REQ-7 真固件升级链 + REQ-12 参数升级均影响现场维护行为）→ **全量档**。出口 B 亦成立（CO-6-P），但本版有可感功能任务，按既有链走 A、CO 搭车。
- **服务阶段目标**：**M4-④**（升级链）＋ **M5-①**（参数版本化；M5 由 P0-5 建）。

### 1.1 输入说明与 CO 逐条处置

输入来源：`versions/v2.29/REVIEW.md`（r1 有条件通过）+ `versions/v2.29/REVIEW-r2.md`（r2 **通过**，SSOT）+ `BLOCKED-2__v2.29…md`（执行侧停手申报）。分级沿用评审登记（未分级按 CO-M 保守）。

| # | 问题摘要 | 来源 | 处置 | 理由 / 触发条件 |
|---|---|---|---|---|
| 1 | CO-1(v2.29)-M Makefile 触碰面未登记 | r1 §5 | **已关闭** | r2 T-1（`04e6471`）清偿，复评 §2 独立复现 |
| 2 | CO-2(v2.29)-M `ex_min.c` 字节数 / 支持矩阵分布失准 | r1 §5 | **已关闭** | r2 T-2/T-3（同批）逐值改正并经复评 |
| 3 | CO-3(v2.29)-M CMake 命令缺编译器前置 + 产物目录忽略 | r1 §5 | **已关闭** | r2 T-4；`.gitignore` 已含 `build-cmake/`（实测） |
| 4 | CO-4 / CO-5(v2.29)-M 留痕件归并 / `--verify` 留痕 | r1 §5 | **已关闭** | r2 T-5；`build/final-gates-v229.log` 六段 + `r5_verify_rc.txt` = RC=0 |
| 5 | CO-6(v2.29)-P REQ-7 真固件双单（下版口径） | r1 §5 | **纳入** P1（P1-1…P1-4） | Owner 裁决「认领 v2.30」，复评 §5 承接行在案；输入 = `BLOCKED-2` 实测三件 |
| 6 | CO-7(v2.29)-M（低）r2 档「末笔」表述残留两处 | r2 §5 | **纳入** P0-1 | 已发布件（tag `v2.29` 含 r2 档）⇒ 按值勘误 **1:1 行内** + 修订登记行；不重写 tag |
| 7 | CO-8(v2.29)-M（低）r2 档 §4 对账口径计数失准 | r2 §5 | **纳入** P0-1 | 同批（同一档两处，可一笔两行） |
| 8 | CO-9(v2.29)-M（低）tag 后收尾笔使 (d) 面退回未登记 | r2 §5（勘误追加） | **纳入** P0-2（条文半边） | token 半边已由 `980210a` 统一登记（十档各 +1 行，逐档复跑 rc=0）；本条只剩**条文**：模板写明「tag 后收尾笔会短暂拉红 (d)，由下一版登记轮吸收」 |
| 9 | `BLOCKED-2`：真固件传输面失步（88 KB / 128 B 块 / block 91 CAN）+ 五条建议 | 执行侧停手申报 | **纳入** P1-1…P1-4 | 三改（1K 块型 / 心跳静默 / 波特率兜底）均**板侧 demo**；库面零改动（`ET_XM_1K` 为库内既有编译开关，`xmodem_send.py` 已有 `--xmodem-1k`） |
| 10 | `.qoder-credits/` 未被 `.gitignore` 覆盖（Qoder 会话产物目录） | v2.29 r2 交付 §5.4 新发现 | **纳入** P0-3 | 一行忽略 + 注释说明；零风险 |
| 11 | v2.29 的 `AT+BOOTINFO` 三字段来自参数区残留、不构成新 confirm 证据 | `BLOCKED-2` §1 行 4 / §3 建议 5 | **纳入** P0-4 | 已发布件（v2.29 交付 AC-8 行）⇒ 按值补注「归因不成立」+ 修订登记；并在本版 P1-3 以「先清状态扇区再 stage」重建归因 |
| 12 | 跨版：`CO-12(v2.28)` 门区间 `--from` / `CO-9①(v2.27)` [自指] fail 诊断行 / `CO-8(v2.8)-P` USART2 双口 / ch32x035 与 f103 挂账 / v3 池 | 跨版链 | **暂缓（维持，重新声明）** | 触发逐条不变；本版板侧窗口 = **仅 G474**；门语义改动维持随 v3 池（本版仅新增模块断言块，判定语义零变更） |
| 13 | REQ-12 = N-03 参数版本化（P1） | 用户提案 + BACKLOG | **纳入** P2-1…P2-3 | 本版第二主题；纯库面、host 可测；schema/迁移/原子保存/恢复出厂保留为必做，**导出/导入与 CLI 延后**（REQ-12 验收 4 的跨设备导入不在本版） |
| 14 | REQ-11（FreeRTOS port 本体）/ REQ-13（黑匣子）/ REQ-14（断点续传+签名）/ REQ-15/16/17 | 用户提案 + BACKLOG | **暂缓** | 触发 = 后续版本认领；本版容量已含板侧窗口 + 新模块；N-04/N-05 依赖本轮 `et_params` 与装备面 |
| 15 | REQ-7 剩余面（传输 + 双单） | BACKLOG | **纳入** P1 | 本版唯一 CO-P；判据 = `BLOCKED-2` 的三项修复 + 双单证据 + 归因纠正 |

### 1.2 本版做什么

1. **P0 勘误与条文批（G0 窗口）**：r2 档两处按值勘误（CO-7/CO-8，1:1 行内 + 修订登记）；「tag 后收尾笔登记形态」条文入两模板（CO-9）；`.qoder-credits/` 入 `.gitignore`；v2.29 交付 AC-8 的 BOOTINFO 归因补注；ROADMAP 的 M4 收尾 + M5 登记。
2. **P1 升级链闭合（REQ-7 / M4-④）**：板侧 demo 三改（`ET_XM_1K=1` + `g_xmbuf[1032]` / 升级期静默 `alive` / 波特率兜底）；**真固件双单**（confirm + 回滚，状态扇区先清以重建归因）；证据 + 实机记录 §20；若仍失步 → BLOCKED 并升级库面任务（触发条件写明）。
3. **P2 参数版本化（REQ-12 / M5-①）**：新模块 `storage/et_params.{h,c}`（第 **36** 模块）——定容 schema 表（key/类型/长度/默认/范围/分组/版本/保留标记）+ 加载/保存/校验/逐版迁移/恢复出厂 + **代际原子保存**（新代际全部写完后以单键 gen 切换为提交点，掉电半写则仍读旧代际）。
4. **P3 常设批与发版**：基线滚动（归档 v2.29 / BASE 切 v2.29 / 发版点复读）、数字回刷同批、登记链收口、`versions/v2.30/DELIVERY.md`、⑧ 机检周期、tag + **推送顺序**（v2.29 先于 v2.30）。

### 1.3 明确不做什么（Non-goals）

- **库面零改动**：`protocol/et_xmodem.*`、`storage/et_bootctl.*`、`port/*` 本版 **diff = 空**（传输修复全部在板侧 demo 与板侧构建旗标）；若 P1 实测仍失步，按 HC-10⑤ 转 BLOCKED 并**在 v2.31 认领库面加固**（本版不得顺手动库）。
- **不做 REQ-14 断点续传/镜像签名**（N-05）= 独立版本；**不做 N-04 黑匣子 / N-06 低功耗 / N-07 HIL / N-08 CLI / REQ-11 FreeRTOS port 本体**。
- **不做参数导出/导入与 CLI**（REQ-12 验收 4 延后；本版验收 1/2/3/5）。
- **不改门判定语义**：`docref.sh` / `gatecheck.sh` / `sizecheck.sh` / `apidump.sh`（除 BASE）零 diff；`docsync.sh` 仅**纯新增** `et_params` 断言块 + 数值字面。
- **不新增板上 selftest 套件**（22/22 维持）；**新增 1 个模块开关** `ET_MODULE_PARAMS`（32→33，属新模块惯例）。
- **不代做 push / tag / Release**（HC-0：v2.29 与 v2.30 的推送顺序归 Owner）。
- **历史文档冻结**：v1.1~v2.28 平铺/versions 档仅登记行 carve-out；本版仅对 v2.29 交付档做 BOOTINFO 补注（HC-4 放行面）与 r2 档两处勘误（P0-1）。

## 2. 硬性约束

- **HC-0 基线异常与推送顺序（本版特有）**：基线 = `tag v2.29` = `810d355`（本地在位）；**该 tag 与 `main` 均未推送**（`origin/main` = `ef339dd`；`git ls-remote --tags origin v2.29` = 0 行）⇒ ① 本版开工**不以推送为前置**（Owner 明示不 push），但必须在交付 §1 首行如实登记「v2.29 Release 待发」；② **推送顺序约束**：v2.29 的 `push main` + `push tag v2.29` 必须在 v2.30 的代码合入 `main` **之前**完成（或同批先 tag 后 main）——否则 Release 构建面与「tag 先定着」纪律冲突；③ 若 Owner 在 v2.30 期间改变主意要 push，执行者按「先 v2.29 后 v2.30」顺序执行并留痕。
- **HC-1 范围封闭（逐文件允许面 + 登记行 carve-out）**：
  - 新增：`storage/et_params.h`、`storage/et_params.c`、`test/test_params.c`。
  - 库面：`et_config.h`（版本宏两行 + `ET_MODULE_PARAMS`）；`Makefile`（`STORAGE_SRC` + `TEST_SRC` 两列表）；`test/test_main.c`（extern + `g_suites` 一处）。**`core/` `algorithm/` `sys/` `protocol/` `drivers/` `debug/` 与 `port/` 全部 diff = 空**；**`et_xmodem`/`et_bootctl` 零改动**。
  - tools：`tools/docsync.sh`（`et_params` 断言块 K 行 + 数值字面）；`tools/apidump.sh`（BASE 两处）。其余四门 + `bench.c` + `board_window.py` **零 diff**（若需动 `board_window.py`，走偏差登记）。
  - 文档面：`README.md`（版本行 + 交付链接行 + 基线名 ×2 + 特性表 + 用例数面 + 支持矩阵不变）、`docs/API_GUIDE.md`（目录 + **新 `### 6.3 et_params 参数版本化与迁移`**（实测 storage 层现至 6.2）+ `## 10` 配置项 + **新 `### 11.16` 配方**（实测现至 11.15）+ 用例数面）、`docs/API_STABILITY.md`（若新增公开类型/函数按 MINOR 追加，无需 §5.1 结构体追加行）、`docs/API_INVENTORY.md`（重生成）+ `docs/API_INVENTORY_v2.29.md`（新增归档）、`docs/architecture.md`（分层框 storage 行 + 选型表 + `35→36` 模块 + 验证金字塔 + 机制门行）、`docs/开发计划模板.md` + `docs/评审记录模板.md`（P0-2 条文）、`docs/v3-candidates.md`（「v2.30 复评记录」节 + G474/ch32x035/f103 v2.30 行）、`.gitignore`（P0-3）、`CHANGELOG.md`、两 `port/*/README.md`（体积行 + host 读数行）、`移植stm32实机记录.md`（**新 §20**，实测现至 §19）、`BACKLOG.md`（REQ-7/12 状态，规划者预改）、`ROADMAP.md`（M4 收尾 + M5，P0-5）、`versions/v2.29/DELIVERY.md`（P0-4 归因补注 + 修订登记）、`versions/v2.29/DELIVERY-r2.md`（P0-1 两处勘误 + 修订登记）、`versions/v2.30/` 三件套。
  - **登记行 carve-out**：P3-3 对历史 OK 档的纯登记行（`1+/0−`）不计偏差（沿 v2.28/v2.29 条文）。
  - 库外：`D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\Src\et_demo.c`（`g_xmbuf` / 心跳静默 / 升级路径）、板侧 `CMakeLists.txt`（`-DET_XM_1K=1`）、证据目录 `build\v2.30-evidence\`。
- **HC-2 git 锚定与触发面**：快照 `snap-2.30-start`（**无 `v` 前缀**）打于入仓批末笔，打后**实测触发面**（`gh run list --limit 3` / `gh release list`，读数入交付——本版因不 push，预期无新 run/new Release，须以实测承载该声称）；分支 `dev/v2.30`。
- **HC-3 证据面**：⑧ 机读值 = `gatecheck 2.30 --emit` 逐字粘贴；`--verify` 绿态 = 定稿链末笔；封闭枚举复测留痕 `build/final-gates-v230.log`（段数 = 声称面）。**对照面**：P1 的三项修复须给出 **改前/改后** 传输读数（块号停止点、ACK 往返比、总耗时），禁止只报"通过"。
- **HC-4 修订登记放行**：本版对已发布件的实质变更 = `versions/v2.29/DELIVERY.md`（P0-4 归因补注）与 `versions/v2.29/DELIVERY-r2.md`（P0-1 两处），二者走「修订登记 @`<sha>`」放行（短语行与 token 行**分行**）；其余历史档仅登记行 carve-out。
- **HC-5 数字回刷同批（门强制面 ⊆ 本清单，逐值标注口径）**：MINOR 30 / `0x021E00`；README 版本行 + 交付链接行 + 基线名前滚 ×2（→ v2.29 归档）；CHANGELOG `[2.30.0]` 日期 = 终态提交日；architecture 机制门行 = N（预置复算 + 终态实测）；**模块数 35→36**（`docs/architecture.md:74` 与 README 特性表行数、CHANGELOG 第 36 模块）；**开关数 32→33**（`grep -c '^#define ET_MODULE_'`）；**用例数**（现值 526 系，四形态以终态实测回填）随 `test_params` 滚动，回刷面 = `grep -rn` 终态全列点名；例数 **6 维持**；两 port README v2.30 体积行构建实测；`API_INVENTORY.md` 重生成。
- **HC-6 契约零破坏**：`apidump --check` exit 0；`apidump --diff` 对 v2.29 基线 = **修改/删除 0 项，新增 = 本版点名清单**（`et_params_*` 全部公开面 + `ET_MODULE_PARAMS` 等宏）；**`et_xmodem` / `et_bootctl` / `port` 既有签名零删改**（`--diff` 点名清单不含这些前缀）；`docs/API_STABILITY.md` 无结构体字段追加（若发生即偏差）。
- **HC-7 门改动边界**：`docsync.sh` 允许面 = ① `et_params` 新模块断言块（**纯新增 K 行**，形态沿 `:334-343` 等既有模块先例）；② P3-2 数值字面。判定语义零变更；同批 architecture 回填 + N 复算 + docref 登记。
- **HC-8 如实性与口径**：① **BOOTINFO 归因**：P1-2 必须先清状态扇区或以 `et_bootctl_stage` 的真实新记录为起点，证明 `staged/confirmed/attempts` 来自**本次**操作；② 传输修复的对照读数按值标注（块号 / 重试次数 / 耗时）；③ 断电注入 100 次的计数口径（注入点分布与失败判据）写明；④ `--ver` 奇偶语义沿既有口径（与固件 MINOR 无关）；⑤ 计数类逐值标注口径，不混用。
- **HC-9 分层与不变式**：`et_params` 属 `storage/`，**只向下依赖** `core/algorithm` 与 `et_kv`/`port`（`docs/architecture.md:28` 单向依赖）；不得依赖 `sys/` `debug/`；零动态内存、多实例句柄化、ISR 标注显式（`et_params` 全部 `🏠MAIN`，因 flash 写与迁移不可 ISR 调用）。
- **HC-10 板侧义务（REQ-7 专项）**：① 修复顺序 = 1K 块型 → 心跳静默 → 波特率兜底（115200→57600→19200），每步实测并记录停止点，**不得跳步**；② 两单判据 = confirm（`BOOTINFO` 三字段自本次 stage 起算 + `AT+VER` 当版）/ 回滚（A 槽存活 + `attempts` 递增且自检失败路径真实触发）；③ 烧前版本串机检不过不得烧录；④ 证据落 `build\v2.30-evidence\`；⑤ **若三改后仍失步**：按 `templates/BLOCKED.md` 停手，登记「库面加固」为 v2.31 触发项，**本版不得改库面绕过**。

## 3. 任务分解

### P0 勘误与条文批（G0 窗口，零库面）

- **P0-1（CO-7/CO-8）r2 档两处按值勘误**：`versions/v2.29/DELIVERY-r2.md` —— ① §4 记录范围的「R4 = 定稿链末笔」改「R5 = 定稿链末笔（`810d355`）」及偏差 4 同处；② §4 对账口径「围栏 4 行 = 行首具名 2 笔 + 首行合写 3 笔」按实测改为「1 行合写（R3/R4/R5）+ 3 行具名」形态（**以复评 §5 的实测值逐项复算后再落笔**）。二者均为 1:1 行内、`wc -l` 不变；同批加修订登记行（token = 本笔 `@<sha>`，短语行与 token 行分行）。判据 = 两处 `grep` 命中新形态、旧串 0 命中（判据串不匹配判据行自身）+ docref 对该档 = 缺声明行（既定形态）。来源 = `CO-7/CO-8(v2.29)`。
- **P0-2（CO-9 条文）tag 后收尾笔登记形态**：`docs/开发计划模板.md` + `docs/评审记录模板.md` 各一行：「tag 后的收尾笔（复评记录入仓 / 状态位代改 / 勘误追加）会使 docref (d) 面短暂退回未登记，属**预期**，由下一版 P4-3 登记轮以纯登记行吸收；tag 不因此重打、`--verify` 以 tag 前定稿链末笔为准」。判据 = 两处各 `grep -c` = 1（纯追加，旧行严格前缀比对）。来源 = `CO-9(v2.29)`。
- **P0-3 `.qoder-credits/` 忽略**：`.gitignore` 增一行 `.qoder-credits/` + 注释（Qoder 会话产物目录）。判据 = `git check-ignore .qoder-credits` 命中 + `git status` 不再列出。来源 = v2.29 r2 交付 §5.4。
- **P0-4（BLOCKED-2 §1 行 4）v2.29 交付 AC-8 归因补注**：`versions/v2.29/DELIVERY.md` AC-8 行后补一句「`AT+BOOTINFO` 三字段读数来自参数区残留（烧录未擦参数区），**不构成本版新做一次 confirm 的证据**」+ 修订登记行。判据 = 该句在位 + 旧声称未被删（纯追加）+ docref 对主档 rc=0。来源 = `CO-6(v2.29)-P` 输入 / `BLOCKED-2`。
- **P0-5（M4/M5）ROADMAP 收尾与新建**：`ROADMAP.md` —— ① M4-④ 标「本版收尾中」并在 P1 达成后结案；② 新建 **M5「参数与故障可维护性」**（窗口 v2.30~v2.33；判据 = ① 参数版本化（REQ-12）/ ② 现场黑匣子（REQ-13）/ ③ 可恢复升级（REQ-14，最小面 = 断点续传或签名其一））；③ 项目状态补 v2.29 未推送 + v2.30 立项。判据 = `^\*\*M5` = 1 + M4 结案行与 P1 实测一致。来源 = `M-5` + Owner 裁决。
- **P0-6 阶段收尾判据登记**：本版 §4 AC-8 通过后，M4 四条全达成 ⇒ `ROADMAP.md` M4 移入「已结案阶段」并写达成证据指针（v2.29 ①②③ + v2.30 ④）。判据 = M4 结案段行在位。来源 = 常设（ROADMAP 口径）。
- **G0 窗口判据**（右端 = G0 末笔 SHA 实测回填）：窗口谓词逐文件枚举既有门（`docref.sh` / `gatecheck.sh` / `sizecheck.sh` / `apidump.sh`）+ 库面六目录 + `port/` + `Makefile` + `et_config.h` = diff **空**；`docsync.sh` 显式排除并归因（P2 计划内）；本批触碰面（`versions/v2.29/DELIVERY*.md` / 两模板 / `.gitignore` / `ROADMAP.md` / `BACKLOG.md`）实测非空并写明归因；docsync G0 终态 = 复算值。

### P1 REQ-7 升级链闭合（板侧；库面零改动）

- **P1-1 板侧 demo 三改（库外）**：`Core\Src\et_demo.c` = ① `g_xmbuf[132]` → `g_xmbuf[1032]`；② `cmd_upgrade` 期间**静默 `alive` 心跳**（含其日志通道与其他非协议输出），恢复点 = 升级返回后；③ 板侧 `CMakeLists.txt` 增 `-DET_XM_1K=1`（库内既有开关，默认 0，**不改库文件**）。判据 = 板侧构建 0 warning + `AT+UPGRADE` 路径下 TX 仅协议字节（抓包/日志实证）。来源 = `REQ-7` / `BLOCKED-2` §3 建议 1-3。
- **P1-2 真固件双单（逐单归因）**：打包当版真固件（body 实测 ≤ 131,072 B；v2.29 实测 87,840 B）→ `xmodem_send.py --xmodem-1k` 真传输 → **confirm 单**（先以 `et_bootctl_stage` 产生本次新记录，再复位/自检通过 → `BOOTINFO` 三字段 + `AT+VER`）→ **回滚单**（`--ver` 偶数触发自检失败 → A 槽存活 + `attempts` 递增）。判据 = 双单证据在档且三字段**来自本次操作**（HC-8①）。来源 = `REQ-7` / `CO-6(v2.29)-P`。
- **P1-3 证据与实机记录**：`移植stm32实机记录.md` 新 **§20**（节号落笔前 `grep -n '^## '` 复核；现至 §19）——含三改前后对照读数（停止块号 / 总耗时 / 重试比）、双单原文、分区表复核；证据落库外 `build\v2.30-evidence\`。来源 = 常设（模板 §7⑤）。
- **P1-4 失败升级路径**：若三改后仍失步 ⇒ `BLOCKED-n` 停手申报 + `docs/v3-candidates.md` 记「板侧 xmodem 大块传输可靠性」候选（触发 = 库面加固认领），并如实登记 AC-8 未达成。来源 = `BLOCKED-2` §3 建议 4 + HC-10⑤。

### P2 REQ-12 参数版本化（`storage/et_params`，第 36 模块）

- **P2-1 模块本体**：`storage/et_params.{h,c}` —— 定容 schema 表（字段：key / 类型（u8/u16/u32/i32/blob 定长）/ 长度 / 默认值 / 范围 / 分组 / `since_ver` / `keep_on_factory_reset`）+ `et_params_t` 句柄（调用方提供 schema 表与工作区，零分配）+ `load/save/validate/factory_reset/migrate/get/set`；**代际原子保存** = 写入新代际全部 key 后以单键 `gen` 切换为提交点，加载时按 `gen` 选完整代际，掉电半写则继续读旧代际（HC-8③ 口径）。API 形状（纯追加）：不触碰 `et_kv` 签名。判据 = AC-3/AC-4。来源 = `REQ-12`。
- **P2-2 单测**：`test/test_params.c` —— 迁移（v1→v2 保旧值/补默认）、迁移失败回滚旧配置、范围校验拒绝、恢复出厂保留 `keep` key、**断电注入 100 次**（沿用 host flash 掉电截断注入）后配置完整、gen 切换点前后一致性。判据 = `make test` 全绿 + 用例可独立复现。来源 = `REQ-12` 验收 1/2/3/5 + 常设（host 可测）。
- **P2-3 文档与门**：`docs/API_GUIDE.md` 新 `### 6.3` + 目录 + `## 10`（`ET_MODULE_PARAMS` 与调参宏）+ 新 `### 11.16` 配方（参数升级与恢复出厂）；`docs/architecture.md` storage 行 + 选型表 + 模块数 36；`README.md` 特性行；`Makefile` 两列表；`test_main.c` 注册；`docsync.sh` 新模块断言块（K 行）；`CHANGELOG` 第 36 模块条目。判据 = 五面机读命中 + docsync 全绿。来源 = 新模块惯例 + 常设（README 清单①）。

### P3 常设批与收口

- **P3-1 基线滚动三件事**（常设，模板 §7⑥）：① `docs/API_INVENTORY_v2.29.md` 归档（worktree 于 tag v2.29 真生成 + sha256 + 与 v2.28 归档 diff 逐段）；② `tools/apidump.sh` BASE 切 v2.29（**实测两处**）；③ 发版点复读 = 修改/删除 0、新增 = 点名清单（HC-6）。
- **P3-2 数字回刷同批**（HC-5 全清单，分两段）：**(a) 版本宏段（先于板侧烧录）**；**(b) 其余数值段（随定稿批）**。
- **P3-3 登记链收口**：以 docref 实测点名面为准追加纯登记行（本版触碰 `docsync.sh` / `storage/et_params.*` / `test/test_params.c` / `Makefile` / 两模板 / `.gitignore` / `versions/v2.29` 两档）；并复核 `980210a` 已登记面仍 rc=0。来源 = 常设（docref）。
- **P3-4 `versions/v2.30/DELIVERY.md`**：沿权威模板 §1–§8 + 行首声明 + 覆盖率行 + §4 记录范围 + ⑧ 逐字片段 + **§8 破坏性候选段** + 同批登记面三件（复评记录节 / G474+ch32x035+f103 v2.30 行 / §6 零裸占位符）。
- **P3-5 定稿链与发版**：定稿 → `--emit` 逐字 → ⑧ 四类复测留痕 → `--verify` PASS @定稿链末笔 → tag `v2.30`（本地）→ **推送顺序**：先 `push main`+`push tag v2.29`（若 Owner 仍未推）→ 再 `push main`+`push tag v2.30`（HC-0②）；CI + Release 核验；删 `dev/v2.30`。若产生 rN 档：沿 CO-8(v2.26) / v2.27 P0-4 / v2.29 CO-9 条文（末笔身份单点承载；tag 后笔由下一版登记轮吸收）。

## 4. 验收标准（全量档；证据落点 = `versions/v2.30/DELIVERY.md` + `移植stm32实机记录.md §20` + `build/` 留痕 + 库外证据目录）

- **AC-0（基线异常）**：交付 §1 首行登记：`tag v2.29` = `810d355`（本地）、`git ls-remote --tags origin v2.29` = 0 行、`origin/main` 落后 N 笔、v2.29 Release 待发；HC-0 推送顺序约束在案。
- **AC-1（P0-1）**：r2 档两处新形态命中、旧串（判据串不匹配判据行自身）0 命中、`wc -l` 不变；修订登记 token 在档（分行形态）；docref 对该档 = 缺声明行（既定）。
- **AC-2（P0-2/P0-3/P0-4/P0-5）**：两模板条文各 1（纯追加前缀比对）；`.qoder-credits` 被忽略实测；v2.29 交付 AC-8 补注句在位且 docref 主档 rc=0；`ROADMAP.md` `^\*\*M5` = 1、M4 收尾与 P1 实测一致。
- **AC-3（P2-1 / REQ-12）**：`storage/et_params.h` 公开面在 `apidump --diff` 点名清单内、修改/删除 0；**代际原子**语义在 host 可复现（gen 切换前后注入断电各态读取到完整代际）；范围校验/恢复出厂保留 key 用例通过。
- **AC-4（P2-2）**：断电注入 **100 次**（注入点覆盖 save 过程各阶段，计数口径写明）后加载全部通过、无半套；迁移失败回滚旧配置且有可读故障记录；`make test` 全绿。
- **AC-5（P2-3）**：五面文档义务逐项机读命中；`docsync` 新模块断言块 K 行在位 + 反向探针（临时树删一行 → 红）；模块数 36 / 开关数 33 实测同值。
- **AC-6（HC-6）**：`apidump --check` exit 0；`--diff` 新增 = 点名清单、修改/删除 0；`et_xmodem`/`et_bootctl`/`port` 前缀零变更；`API_STABILITY` 无结构体追加。
- **AC-7（P1-1）**：板侧构建 0 warning + `-DET_XM_1K=1` 生效（传输日志/块型实证）；升级期间 TX 无 `alive` 等非协议输出（抓包/日志对照）；三改前后停止块号对照读数在档。
- **AC-8（P1-2 / REQ-7，M4-④）**：**真固件双单**证据在档——confirm（本次 stage 起算的 `staged/confirmed/attempts` + `AT+VER`）与回滚（A 槽存活 + `attempts` 递增 + 自检失败路径真实触发）；镜像 body 当版真固件（体量 ≤ 槽容量实测比较）；`--ver` 奇偶逐处标注；**归因纠正在案**（P0-4）。三改后仍失步 ⇒ 本 AC 记未达成 + `BLOCKED-n` 在档（不得以合成镜像冒充）。
- **AC-9（P3-1）**：`API_INVENTORY_v2.29.md` 存在 + blob sha256 + 与 v2.28 归档逐段 diff；`apidump.sh` BASE 两处实测；发版点复读形态正确。
- **AC-10（P3-2）**：MINOR 30 / `0x021E00`；README 版本/链接/基线名；模块 36 / 开关 33；用例数 `grep -rn` 终态全列同值；CHANGELOG `[2.30.0]` 日期 = 终态提交日；体积行实测。
- **AC-11（全量回归与发布面）**：四形态读数实测（基线 526 系）+ `make ex` 六配方 PASS；`docsync` N/N；`apidump --check` exit 0；`sizecheck` 3/0；`docref` 逐档点名清单；CI 七 job（若 Owner 推送）；快照触发面实测（本版预期无新 run）。
- **AC-12（⑧ 与 (d)）**：⑧ 五锚行 = `--emit` 逐字；`--verify` PASS @定稿链末笔；(d) 全集（v2.29-r2 终态 37 份 + 本档，以发版点机读为准）；token 机读与 §7.3 一致；**tag 后收尾笔的 (d) 短暂红**按 P0-2 条文登记为预期。
- **AC-13（G0 窗口）**：窗口谓词于 G0 终态实测 = 空；`docsync.sh` 排除归因 + 触碰面归因在案；库面六目录 + `port/` + `Makefile` + `et_config.h` 零 diff（除 P2 计划内）。
- **AC-14（依据面）**：`docs/v3-candidates.md`「v2.30 复评记录」节 + 三板 v2.30 状态行 +（若 P1 失败）xmodem 可靠性候选条目。

## 5. 可裁量项与不可偏离

- **可裁量**：① `et_params` 的代际键命名与 schema 表形态（字段不可少）；② 迁移接口形态（函数表 vs 单回调 switch，须逐版本步进可测）；③ `et_params` 内部是否复用 `et_kv` 的 `commit/iter`；④ 板侧三改的具体实现（心跳静默用标志位 or 日志级别；波特率兜底的自动/手动试）；⑤ `test_params` 用例划分；⑥ 提交切分与 (d) 点名面；⑦ DELIVERY 章节编排。
- **不可偏离**：§1.1 全部处置；§2 全部 HC——尤其 **HC-0②**（推送顺序）、**HC-1**（库面零改、`et_xmodem`/`et_bootctl`/`port` 零 diff）、**HC-8①**（BOOTINFO 归因）、**HC-10**（三改顺序 + 双单判据 + 失败升级路径）；§1.3 Non-goals；§6 基线。

## 6. 基线

| 字段 | 内容 |
|---|---|
| 首选基线 | **`tag v2.29` = `810d355`**（本地实测在位，`main` 祖先） |
| 基线异常登记 | tag 未推送（`git ls-remote --tags origin v2.29` = 0 行）、Release 未生成、`main` 未推（`origin/main` = `ef339dd`）；Owner 2026-10-21 明示不 push ⇒ 本版以本地 tag 起线，**不阻塞**；推送顺序见 HC-0② |
| 入仓批（预申报） | `BACKLOG.md`/`ROADMAP.md` 规划者预改 + 本计划（+ 若 Owner 决定，`versions/v2.29/REVIEW-r2.md` 勘误已在 `e4a8a20` 入仓）；`snap-2.30-start` 打于入仓批末笔并实测触发面 |
| 快照 | `git tag snap-2.30-start <入仓批末笔>`（**无 `v` 前缀**） |
| 分支规则 | 改动在 `dev/v2.30`；发版 tag `v2.30` 在全部定着后打（本地）；推送与 Release 归 Owner |
| 顺序规则 | ① 入仓批 + 快照 → ② P0（G0）→ ③ P2-1/P2-2（库面模块 + host 测试）→ ④ **P3-2(a) 版本宏** → ⑤ P1（板侧三改 + 双单 + §20）→ ⑥ P2-3 文档与门 → ⑦ P3-1/P3-2(b)/P3-3/P3-4 → ⑧ P3-5 定稿链/tag/（若获授权）推送。板不可达时：⑤ 转 BLOCKED + 挂账续期，其余可先行；**禁止**以合成镜像或干跑冒充板侧绿 |
| 偏离申报 | 实际起点 = 入仓批末笔（≠ `810d355`）属预申报形态；**v2.29 未推送/Release 未发** 为本版已登记偏差（AC-0） |

## 7. 起草期核验清单（六项全过）

| # | 检查项 | 本计划执行情况 |
|---|---|---|
| ① | 内部 tag 命名 | `snap-2.30-start` 无 v 前缀；触发面实测义务入 HC-2（本版预期无 push ⇒ 读数形态 = 无新 run，须以实测承载） |
| ② | "无副作用/不会触发"类断言逐条实测 | 起草期实测（2026-10-21，A1 对话）：`tag v2.29` = `810d355`（本地、`main` 祖先）、`git ls-remote --tags origin v2.29` = **0 行**、`origin/main` = `ef339dd`、`main` = `980210a`（本地领先 25 笔）；`sh tools/docsync.sh` = **384/0**（本机实跑）；`sh tools/docref.sh` 对十档 = **rc=0 提示 ×10**（`980210a` 登记后实测 3 档 + 全量逐档在案）；`ET_VERSION_MINOR` = **29**（`0x021D00`）；`grep -c '^#define ET_MODULE_'` = **32**；`docs/architecture.md:74` = **35 模块**、`:112` 用例 **526**；`examples/ex_*.c` = **6**；`docs/API_GUIDE.md` storage 层现至 **`### 6.2`**、配方现至 **`### 11.15`**（新节号 6.3 / 11.16 未占用）；`移植stm32实机记录.md` 现至 **§19**（新节号 §20 未占用）；`et_kv` 公开面 = `init/format/set/get/del/size/commit/stats/iter_*`（`storage/et_kv.h:73-115`）；`BLOCKED-2` 实测：body **87,840 B** / img **87,872 B** / 槽 **131,072 B** / 停止点 **block 91 ≈ 11.6 KB** / 128 B 块 **687 块**；板侧 demo `g_xmbuf[132]`（`:105`）、`cmd_upgrade`（`:340`）、`alive` 心跳（`:1170-1173`）；`protocol/et_xmodem.h:44-45` `ET_XM_1K` 默认 0；`tools/xmodem_send.py` 支持 `--xmodem-1k`。转交付/终态实测：N、四形态、体积、⑧ 锚值、板侧全部读数 |
| ③ | 板侧落点标注 | 私有文件全路径：`D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\Src\et_demo.c`（`g_xmbuf`/心跳/升级路径）、`D:\code\STM32CubeMX\G474VET6_ET_TEST\CMakeLists.txt`（`-DET_XM_1K=1`）、同步目录 `...\Core\et\`、证据 `...\build\v2.30-evidence\`；**禁止**写成库内 `examples/`；库内 `examples/` 本版零改动 |
| ④ | AC ↔ 冻结 API 形状互洽 | ① `et_params` 为**纯新增**（新类型/函数/宏）⇒ `--diff` 点名清单形态与实现一致；② 既有 `et_kv` 签名零改（AC-3 断言对象 = 新 API 行为，不涉 `et_kv` 形状）；③ 代际原子语义（gen 单键切换）与 AC-3「断电各态读到完整代际」可同时成立；④ 板侧三改不触库 API（`ET_XM_1K` 为既有编译开关）⇒ AC-7 断言对象 = 构建旗标与 TX 字节面；⑤ 模块数 36 / 开关 33 与"新增 1 开关"实现一致 |
| ⑤ | 证据落点章节号 + 占位节清理 | 板侧证据 = `移植stm32实机记录.md` **新 §20**（实测现至 §19，落笔前再 grep 复核）；API_GUIDE 新 `### 6.3` / `### 11.16`（实测未占用）；「v2.30 复评记录」节落笔前 `grep -n '^## '` 复核（追加即复测）；本版不取代旧占位节 |
| ⑥ | 基线滚动三件事 | P3-1 三件：归档 v2.29（worktree 于 tag v2.29 真生成 + sha256 + 与 v2.28 归档逐段）/ BASE 切 v2.29（两处实测）/ 发版点复读（修改删除 0 + 新增点名清单） |

---

**起草期合规声明**：六项已于起草对话（2026-10-21）执行，实测命令与结果全在 §7②（tag/远端态、docsync 384/0、docref 十档 rc=0、版本宏/开关/模块/用例/例数、API_GUIDE 与实机记录节号、et_kv 公开面、`BLOCKED-2` 体量与停止点、板侧 demo 三处行号、`ET_XM_1K` 与 `--xmodem-1k` 位面）。**规划产出件写出后复跑**：本计划与 BACKLOG/ROADMAP 预改落盘后 `sh tools/docsync.sh` 复跑 = **384/0**（规划者不扰动门）。

**预置数值按值声明与逐项复算（沿用 CO-10(v2.28) 修正后的 N 公式）**：
- docsync N 预置 = 上版终态实测 **384** + **1**（本版 DELIVERY 覆盖率行入循环）+ **0**（本版 rN 文件数，预期）+ **1**（CHANGELOG `[2.30.0]` 链）+ **K**（本版新增断言：`et_params` 断言块，预期 9~12，以 diff 实测）− **1**（上版 rN 档 1 份的占位符拦截面退场）= **385 + K**；**以终态实测回填，禁预写**。
- 用例数基线 = **526**（四形态以 v2.29 终态实测为准：526/526/533/527，本版以 `make test` 终态回填）；模块数 **35→36**、开关数 **32→33**（行计数口径）；例数 **6 维持**；板上套件 **22 维持**。
- 体积行 = `arm-none-eabi-size` 构建实测回填（新模块入 storage 致 .text 增，禁沿用 v2.29 值）。
- (d) 全集 = v2.29-r2 终态 **37 份** + 1 = **38 份**（分类值以发版点 `--emit` 机读为准）。
- 版本宏 = MINOR **30** / hex `0x021E00`（现值 29 / `0x021D00` 实测）；基线 `tag v2.29` = `810d355`（实测；未推送已登记）。
- 容量面（P1 判据）= body **87,840 B** ≤ 槽 **131,072 B**（余 43,200 B）——实测值，构建后复测。

**纪律自查**：判据区间右端时点化；G0 窗口逐文件枚举既有门；登记短语行不携带 token（分行）；引用失实原文入 code span；`--emit` 逐字 + `--verify` 定稿链末笔；计数逐值标注口径；**tag 后收尾笔的登记形态按 P0-2 条文执行**；板侧三改须给改前/改后对照读数，禁只报"通过"；`--ver` 奇偶语义沿既有口径。

## 变更记录（冻结后只允许追加，需 Owner 批准）

| 日期 | 变更 | 原因 | 批准人 |
|---|---|---|---|
| 2026-10-21 | 初稿并即冻结：四路盘点 → 出口 A「升级链闭合 + 参数版本化」全量档；§1.1 行 5-15 逐条承接 v2.29 CO 与 `BLOCKED-2`；HC-0 新增「基线未推送 + 推送顺序」形态；M4 收尾 + M5 新建（P0-5）；BACKLOG/ROADMAP 规划者预改 | Owner 三裁决（组合 / M4+M5 / 只板侧修）+「不进行 push，继续下一版」；批准依据 = standing 规则「规划下一版 = 默认批准」 | Owner（2026-10-21） |
