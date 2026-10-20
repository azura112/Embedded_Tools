# v2.29 开发计划：首次接入、并发契约与真 A/B 分区（功能版）

> 填写：规划者（阶段 2，A1 对话 2026-10-20）。批准依据 = Owner standing 规则「当我让你规划下一个版本时，默认批准」（2026-10-08 明示）→ 交付即冻结。冻结后执行者只打勾；变更只走文末变更记录（需 Owner 批准）。
> 布局：三件套沿 `versions/v2.29/`。权威模板 = `docs/开发计划模板.md`；评审侧 = `docs/评审记录模板.md`。
> 本版输入含**用户侧原件** `新功能提案.md`（仓库根，2026-10-20，第一次使用本库的嵌入式工程师撰写）；其 §5 末段明确要求保留「我的场景」与「验收标准」原文——本计划以 REQ-10…REQ-18 逐条映射，原件随入仓批入库、不删改。

- 状态：**已批准-冻结**（Owner standing 规则；§7 六项已过，文末合规声明在案）
- 判据档位：**全量档（功能版，出口 A）**
- 基线 commit：**首选 `tag v2.28`——起草时点实测不存在**（`git tag --list "v2.2*"` 止于 v2.27；`main` = `5d33d2f`，`dev/v2.28` = `ef339dd` 领先 35 笔且 `HEAD..main` = 0）→ 开工硬前置见 **HC-0** 与 §6（Owner 裁决 = 先补 v2.28 发版与两处未闭合项，再以 tag 起线）
- 版本目标：v2.29.0（`ET_VERSION_MINOR` 28→29，hex `0x021D00`；实测现值 28 / `0x021C00`，`et_config.h:20/:22`）
- 服务阶段目标：**M4-①②③④**（M4「首次接入与产品化路径」由 P0-6 登记；M3 于本版判定收尾）

- **价值声明（行为级，用户视角）**：本版交付后——① **一个第一次用本库的工程师能在现有 CMake 工程里用 5~10 行接入**（`add_subdirectory`/`find_package` 二选一），交叉编译零警告，关掉的模块在 `nm`/map 里没有符号，MINOR 升级接入代码不用改；② 干净机器上**按一页纸 10 分钟**跑通 ringbuf + KV 的 host 最小样机；③ **法务一个工作日能判「可以用」**（MIT + 版权行），并有一张写明「验证过 / 未验证 / 不支持」的支持矩阵（编译器、标准、RTOS、平台，逐行带验证方式）；④ 每个模块的**并发安全级别可查**（ISR 可调用 / 任务可调用 / 单所有者 / 多生产者），不用猜；⑤ 升级链在**真分区**上成立：G474 真 A/B 分区落地后，当版真固件双单（confirm / 回滚）实跑（此前 48,140 B 真固件装不进 32 KB 参数区窗口，物理不可达 = `BLOCKED-2`）。

## 1. 目标与范围

### 1.0 价值声明与立项判定声明

- **四路盘点（2026-10-20 判定对话实测）**：

| 路 | 结果 |
|---|---|
| ① 用户直连 | `新功能提案.md` = **3 项采纳前置 + N-01…N-08**（P0 = 构建接入；P1 = FreeRTOS 适配 / 参数版本化 / 现场黑匣子 / 可恢复升级；P2 = 低功耗 / HIL；P3 = 可观测 CLI）。起草期实测核对：`LICENSE*`/`COPYING*` **不存在**；仓库级无 `CMakeLists.txt`（`find -maxdepth 2` 仅板侧私有工程有）；C99 仅在 `README.md:3` / `docs/getting-started.md:4` 声明；`README.md:172` 只有「已验证平台」一行，**无编译器/标准/RTOS/平台的验证-未验证-不支持矩阵**；FreeRTOS 全库近乎零提及。Owner 本对话四项裁决：出口 A「接入版」认领范围 / **LICENSE = MIT**（版权人 = `git config user.name` 实测 `azura`）/ 先补 v2.28 发版再取 tag 基线 / 新建 M4 承接提案 |
| ② BACKLOG.md | **REQ-7 = 已由 Owner 裁决转 v2.29**（路径 A：板侧真 A/B 分区，契约面；v2.28 评审 §6 T-6 据此判不适用）；REQ-5 已纳入 v2.28、**走单面未闭合**（v2.28 未闭合项，随 HC-0 处理）；REQ-2/3/4 暂缓（触发未现：v2.27/v2.28 板侧实测 COM16/WCH-Link 不在位）；REQ-6/8/9 已交付（状态位待 v2.28 复评，不代转）。**新增 REQ-10…REQ-18** = 本提案逐条映射（行 15-23），规划者已预改 BACKLOG。速记区空 |
| ③ 最近一版评审记录 CO 存量 | 输入源 = `versions/v2.28/REVIEW.md` §5（2026-10-10，有条件通过；**CO-1…CO-12**）+ r2 交付 `DELIVERY-r2.md` §5（**无 `REVIEW-r2.md`** ⇒ 按模板 §1.1 显式声明缺失，r2 期新发现自赋号 `RC-1…RC-5` 人工回收）。**CO-P ×3**：CO-1（`board_window.py` abort 判据，r2 R6 已修，本机复核 `:154/:162/:193-197` 残差分列在位）、CO-7（REQ-7 体量/槽几何 → 本版 P2）、CO-8（内存门可达性，**待 Owner 三择一 = HC-0 前置④**）。**CO-M ×9**：CO-2/3/4/5/6′/9/11 已由 r2 清偿（本机抽检：池节 `docs/v3-candidates.md:629` 在位、`README.md:5` 链接已指 `versions/v2.28/DELIVERY.md`、留痕件 `build/final-gates-v228.log` 15,132 B 在位）；CO-10（评审侧时点纪律）与 r2 回收的条文类并入本版 P0；CO-12（门区间右端）维持 v3 池 |
| ④ ROADMAP.md 未达判据 | **M3-①②③④ 达成在档**；**M3-⑤ 未闭合**（`board_window.py --full` 第⑤步 `serial.Serial(COM12)` 抛 `PermissionError(13)` = 烧录后枚举竞态；需 Owner 复跑窗口）→ 本版判定收尾（P0-6 归 M3 并对 ⑤ 结局如实定着）；**M4 新建**承接本提案；M1 已结案；M2（v3）触发未现（本提案按 Owner 裁决走 M4，不触 v3） |

- **三出口与档位声明**：**出口 A 功能版**——判据 = ≥1 项用户可感差异任务（N-01 决定「能不能开始」，来源 = 用户直连原件 REQ-10）→ **全量档**。出口 B 亦成立（CO-P ×2 可执行：CO-7/CO-8），但本版有可感功能任务，按既有链走 A，CO 搭车。
- **服务阶段目标**：M4-①②③④（判据与 REQ 逐条对应，行 15-23）＋ M3 收尾。

### 1.1 输入说明与 CO 逐条处置

输入来源：`versions/v2.28/REVIEW.md`（2026-10-10，有条件通过；§5 = SSOT）+ `versions/v2.28/DELIVERY-r2.md` §5（r2 未核验项与新发现，人工回收）。**显式声明缺失**：`versions/v2.28/REVIEW-r2.md` **不存在**（`versions/v2.28/` 实测四件 = PLAN/DELIVERY/DELIVERY-r2/REVIEW）⇒ r2 期新发现自赋号 `RC-1…RC-5`（行 10-14），处置与 CO 同权重。分级沿用评审登记（未分级按 CO-M 保守）。

| # | 问题摘要 | 来源 | 处置 | 理由 / 触发条件 |
|---|---|---|---|---|
| 1 | CO-1(v2.28)-P（中）`board_window.py` 同步要素 abort 判据混算拷前差异与拷后残差（首次同步必中止） | 评审 §5 | **已关闭** | r2 R6（`78aee9c`）T-2 已修；本机复核 `tools/board_window.py:154/:162` 文本与 `:193-197` `residual` 分列在案；真板 `--full` 走单面仍属 v2.28 未闭合（HC-0③） |
| 2 | CO-2(v2.28)-M（中）P4-4「同批登记面三件」缺二（池节 / 状态行） | 评审 §5 | **已关闭** | r2 R7（`be6a40f`）建「v2.28 复评记录」节（`docs/v3-candidates.md:629` 实测）；G474/ch32x035/f103 v2.28 行随批 |
| 3 | CO-3(v2.28)-M（中）README 交付链接行未回刷而 AC-11 自称已刷 | 评审 §5 | **已关闭** | r2 R9（`f158d05`）改 `README.md:5`（实测已指 `versions/v2.28/DELIVERY.md`）+ 新增链接断言（K=1） |
| 4 | CO-4(v2.28)-M（低-中）按值声称失准两例（`CO-7(v2.26)` 族第五、六例） | 评审 §5 | **已关闭** | r2 R3（`ab4d104`）T-5 四处严格 1:1 逐值改正（numstat 5+/1− 形态在档） |
| 5 | CO-5(v2.28)-M（低）权威模板条文编号断裂与标点残留 | 评审 §5 | **已关闭** | 随 r2 R3 T-5 同批（1:1 行内） |
| 6 | CO-6′(v2.28)-M（低）定稿链闭合面三点（2026-10-11 勘误后形态） | 评审 §5 | **已关闭** | r2 R8（`1b03bd6`）T-1 三点落地；⑧ 五行已回填（`8a1f060`），r2 末笔三点链 rc=0 在档 |
| 7 | CO-7(v2.28)-P（中）+ 条文 CO-M **起草期可行性核验未含「镜像体量 vs 目标槽容量」维度** ⇒ REQ-7 判据物理不可达 | 评审 §5 | **纳入** P2 + P0-4 条文 | REQ-7 经 Owner 裁决走路径 A（板侧真 A/B 分区）入本版；**「体量 vs 容量」升为模板 §7④ 的必填维**（含算式与实测读数，见 P0-4 与本版 §7④ 自证） |
| 8 | CO-8(v2.28)-P（中）内存类缺陷机检可达性缺口（本机无 libasan，CI 只在 push/PR ⇒「本地全绿、发版后 CI 红」结构性） | 评审 §5 | **暂缓（触发 = Owner 三择一）** | 属 **HC-0 前置④**：Owner 在 v2.28 补口窗内三择一（本机装 ASan / 门降级为"CI 内声明" / 接受结构性红并登记）。裁决若要求本版落地，则本版按裁决补任务；未裁前不预写处置 |
| 9 | CO-9(v2.28)-M（低-中）交付 §7.1 中间态读数链一处不可复现 | 评审 §5 | **已关闭** | r2 R3（T-2）改逐值形态（AC-9 增幅 = 各 +23，中间态四时点读数在档） |
| 10 | CO-10(v2.28)-M（低）评审侧时点纪律缺陷（读 DELIVERY 与 ⑧ 态早于被评审末笔） | 评审 §5 | **纳入** P0-4 | 条文入两模板：**评审记录落笔前重取 HEAD/⑧ 态/区间三要素，并标注「取数时点 SHA + 落笔时点 SHA」** |
| 11 | CO-11(v2.28)-M（低-中）交付 AC-12 声称的留痕件内容面 > 实际留痕面 | 评审 §5 | **已关闭（复核）** | r2 R15 补段（留痕件 `build/final-gates-v228.log` 15,132 B 在位）；「声称面 ⊆ 物证面」条文随 P0-4 固化 |
| 12 | CO-12(v2.28)-M（低）`gatecheck` (b) 区间右端 = HEAD，与停手申报/评审记录入库交互缺口 | 评审 §5 | **暂缓（维持 v3 池）** | 判定语义变化（`--from <sha>`），随 M2/v3 三条门修法同批；本版**不动门判定语义**（仅 P0-4/P1-2 纯新增断言） |
| 13 | **RC-1**（r2 回收）`tools/board_window.py` 的 `walk()` 串口异常未捕获 ⇒ 端口占用/枚举竞态直接 traceback 退出，而非走「abort 判据 + 留痕」通道（HC-8① 精神面） | `DELIVERY-r2.md` §5.4 | **纳入** P3-2 | 修法 = `except serial.SerialException` 归入 `abort_reason` 并保留已生成件；属 `tools/` 行为面，随本版板侧窗口同批 |
| 14 | **RC-2**（r2 回收）`docref.sh:75`（声明行提取）与 `:130`（token 提取）对围栏/表格行处理不对称 ⇒ 引用他档声明行会被误捕、混 (d) 分类 | `DELIVERY-r2.md` §5.7 | **暂缓（维持 v3 池）** | 判定语义变化，随 `CO-8(v2.26)` 三条门修法同批；本版规避纪律 = 引用他档声明行只写三数与命令、不复制可命中正则的完整形态（沿 v2.28 §5.7 现网规避） |
| 15 | **RC-3**（r2 回收）申报/BLOCKED 件命中 docref 白名单仅因文件名含「评审记录」四字（巧合性豁免） | `DELIVERY-r2.md` §5.3 | **纳入** P0-5 | 一行级条文入 `docs/开发计划模板.md` 与 `docs/评审记录模板.md`：申报件/停手件的白名单归属写明（判定语义变化随 v3 池，本版只落文档） |
| 16 | **RC-4**（r2 回收）主档 §8 无「新增破坏性候选」段 ⇒ 三落点差异面 | `DELIVERY-r2.md` §5.5 | **纳入** P4-4 | 本版 DELIVERY §8 显式含该段（无候选写「无」），并把「该段落点义务」写入模板条文（P0-4 同批） |
| 17 | **RC-5**（r2 回收）末笔身份散写九处；R12→R15 四笔 1:1 补充笔每笔全量顺延（可机检难阅读，逼近铁律 8 熔断） | `DELIVERY-r2.md` §6 | **纳入** P0-4 | 条文裁决 = **围栏首行为唯一权威，其余处只写「见 §4 首行」**；并给 1:1 补充笔设上限（超出即转 r3/复评）。不改门、不动历史档 |
| 18 | REQ-7（P3→本版）真固件 IAP 走单常设化（v2.28 已纳入但物理受阻） | BACKLOG + `CO-7(v2.28)` | **纳入** P2-1…P2-4 | Owner 裁决路径 A：板侧真 A/B 分区（契约面）。容量算式见 §3 P2-1（512 KB flash / 2 KB 页 / 64 KB×2 槽 / 参数区尾 32 KB / 当版真固件 48,140 B） |
| 19 | REQ-10 = N-01 现有构建系统直接接入（P0） | `新功能提案.md` §3 N-01 | **纳入** P1-1…P1-3 | 本版核心可感项；判据照用户原文（5~10 行接入 / 交叉零警告 / 关模块无符号 / install+find_package / MINOR 升级不改接入代码） |
| 20 | REQ-11 = N-02 FreeRTOS 适配与并发契约（P1） | 提案 §3 N-02 | **部分纳入** P3-1（**仅并发契约表**） | 本版容量只够文档面；FreeRTOS port 本体与压测（真 FreeRTOS 工程 / 优先级 / 嵌套）**暂缓，触发 = 下一版认领**（本版支持矩阵里 FreeRTOS 行如实写「未验证」） |
| 21 | REQ-12 = N-03 参数版本化与迁移（P1） | 提案 §3 N-03 | **暂缓** | 新模块（schema/迁移/掉电安全），工作量 > 本版容量；触发 = M4 后续版本认领（本版支持矩阵与文档不声称具备） |
| 22 | REQ-13 = N-04 现场黑匣子（P1） | 提案 §3 N-04 | **暂缓** | 新模块 + port 复位原因/fault 寄存器面；触发 = M4 后续版本（依赖 port 契约扩展的下一轮） |
| 23 | REQ-14 = N-05 可恢复、可回滚升级（P1） | 提案 §3 N-05 | **暂缓（部分依赖本版）** | 断点续传/签名/会话与传输解耦 = 独立版本；本版只完成其物理前提（真 A/B 分区） |
| 24 | REQ-15 = N-06 低功耗协调（P2） / REQ-16 = N-07 HIL（P2） / REQ-17 = N-08 可观测 CLI（P3） | 提案 §3 | **暂缓** | 触发 = M4 后续或用户侧需求升级；本版不认领（容量与依赖顺序：HIL 依赖 N-01 的 CMake 面，CLI 依赖 metrics 采集面已在 v2.27） |
| 25 | REQ-18 = 采纳前置三件（LICENSE / 支持矩阵 / 10 分钟样例） | 提案 §1 | **纳入** P0-1…P0-3 | 提案明示「缺任一件不进入功能评估」；许可证类型 = **MIT**（Owner 裁决），版权人 = `azura`（`git config user.name` 实测） |
| 26 | 跨版承接：`CO-8(v2.8)-P` USART2 双口 / `CO-4/9(v2.8)-M` / `CO-7(v2.7)-M` / ch32x035 与 f103 挂账 / v3 池 | 跨版链 | **暂缓（维持，重新声明）** | 触发逐条不变；本版板侧窗口 = **仅 G474**；v3 池并入 CO-12、RC-2 两条门语义类 |

### 1.2 本版做什么

1. **P0 采纳前置与条文批（G0 窗口）**：`LICENSE`（MIT）；支持矩阵（`README.md` 新节 + `docs/getting-started.md` 链接，逐行带验证方式）；10 分钟样例（新 `examples/ex_min.c` + getting-started 一页纸）；模板条文四条（体量/容量维、评审时点三要素、申报件白名单、末笔身份单点承载 + §8 破坏性候选段落点）；`ROADMAP.md` 登记 M4 并收尾 M3。
2. **P1 N-01 CMake 接入**：根 `CMakeLists.txt` + `cmake/` 安装导出（`et::embedded_tools`）+ 模块开关映射为 CMake option + host/cross 双构建 + 与 Makefile 的**双源一致性机检**（新增 docsync 断言）+ 文档。
3. **P2 REQ-7 真 A/B 分区**：`port` 区域访问契约（**纯追加**，既有 `port_flash_*` 不变）+ host/G474 实现 + f103 显式「不支持」声明 + G474 私有工程分区（库外链接脚本与配置）+ **当版真固件双单**（confirm/回滚）走单。
4. **P3 并发契约表与工具修复**：`docs/concurrency.md`（35 模块 × 4 列，来源 = 各头文件既有 ISR/上下文标注）+ `board_window.py` 串口异常修复。
5. **P4 常设批与发版**：基线滚动（归档 v2.28 / BASE 切 v2.28 / 发版点复读）、数字回刷同批、登记链收口、`versions/v2.29/DELIVERY.md`、⑧ 机检周期、tag + CI + Release。

### 1.3 明确不做什么（Non-goals）

- **不做 FreeRTOS port 本体**（REQ-11 只做契约表）；**不做 N-03/N-04/N-05/N-06/N-07/N-08**（行 21-24）；不做镜像签名/断点续传（REQ-14 面）。
- **不改语言标准**：仍 C99；C11/C++ 混编只在支持矩阵里标「未验证」，不声称支持。
- **不做新构建系统之外的接入面**：不做 Meson/Bazel；`Makefile` 仍是 host 构建与门禁的既有单一入口（CMake 为**新增**接入面，二者由机检对齐，不互相取代）。
- **不动两板工程本体**：ch32x035 / f103 本版零操作（挂账续期）；G474 只动私有分区配置与 demo（库外）。
- **不改门判定语义**：`docref.sh` / `gatecheck.sh` / `sizecheck.sh` 零 diff；`apidump.sh` 仅 BASE 实测两处；`docsync.sh` 仅**纯新增**断言（P1-2 双源一致性 + P3-1 契约表行数）与数值字面。CO-12 / RC-2 的门语义修法维持 v3 池。
- **不新增板上 selftest 套件**（22/22 维持）；**不新增 `ET_MODULE_*` 开关**（32 维持；REQ-7 的 port 面用既有模块门）。
- **不代做 v2.28 发版动作**（合并/tag/push/Release = Owner/B 侧；HC-0 判据），**不代转** REQ-6/8/9 状态位（判据 = v2.28 复评通过），**不改** `versions/v2.28/REVIEW.md` 勾选框（铁律 5）。
- **历史文档冻结**：v1.1~v2.24 平铺档零触碰；`versions/v2.25/`~`versions/v2.28/` 零内容改动（仅 HC-1 登记行 carve-out）。

## 2. 硬性约束

- **HC-0 开工前置（v2.28 闭环，Owner 裁决承载）**：① v2.28 真板走单复跑窗（`board_window.py --full` 第⑤步成功、五要素对照表取数；M3-⑤ 判据）；② `CO-8(v2.28)-P` 三择一裁决落地；③ 合 `dev/v2.28`（`ef339dd`）入 `main`；④ `git tag v2.28` 落 **`ef339dd`**（v2.28 评审 §8(a) 支 = r2 定稿链末笔；**不是 `8a1f060`**——R14 勘误后口径）；⑤ CI 七 job + Release 四附件 + `dev/*` 清空。任一未完成 ⇒ B1 不得开工，按 `templates/BLOCKED.md` 停机申报（push/tag/Release 属 Owner/B 侧，Agent 不代做）。判据 = `git tag --list v2.28` 非空 且 `git rev-parse v2.28^{}` = `ef339dd`（实测回填）。
- **HC-1 范围封闭（逐文件允许面 + 登记行 carve-out）**：
  - 新增：`LICENSE`、`CMakeLists.txt`、`cmake/*`（install/export 模板）、`examples/ex_min.c`、`docs/concurrency.md`。
  - 库面：`port/port.h`（区域 API 纯追加）+ `port/host/port_host.c`（region 实现）+ `port/stm32g474/port_stm32g474.c/.h`（region 实现）+ `port/stm32f103/*`（仅显式「不支持」声明宏，若需）+ `storage/et_bootctl.h/.c`（`et_bootctl_cfg_t` 尾部追加 + 语义文档；无签名删改）+ `test/test_bootctl.c` / `test/test_kv.c`（或新增 region 单测落 `test/`）+ `test/test_main.c` 仅注册形态变化时。**`core/` `algorithm/` `sys/` `protocol/` `drivers/` `debug/` 六目录 diff = 空**（N-02 只出文档、N-01 只出构建）。
  - tools：`tools/board_window.py`（RC-1 异常面）+ `tools/docsync.sh`（P1-2/P3-1 纯新增 K 行 + P4 数值字面）+ `tools/apidump.sh`（BASE **实测两处**）。`docref.sh` / `gatecheck.sh` / `sizecheck.sh` / `bench.c` / 其余 `tools/*.py` 零 diff。
  - 根/文档：`et_config.h`（仅版本宏两行）、`README.md`（版本行 + 交付链接行 + 基线名 ×2 + 支持矩阵新节 + 用例数 + 例数面）、`docs/getting-started.md`（一页纸样例 + §0 环境准备 + 用例数）、`docs/API_GUIDE.md`（适用版本 + 区域 API 与 bootctl 追加语义 + 用例数面）、`docs/API_STABILITY.md`（§5.1 追加登记行）、`docs/API_INVENTORY.md`（重生成）+ `docs/API_INVENTORY_v2.28.md`（新增归档）、`docs/architecture.md`（分层/选型/验证金字塔/机制门行/例数）、`docs/开发计划模板.md` + `docs/评审记录模板.md`（P0-4 四条条文）、`docs/v3-candidates.md`（「v2.29 复评记录」节 + G474/ch32x035/f103 v2.29 行）、`CHANGELOG.md`、两 `port/*/README.md`（体积行 + host 读数行 + g474 分区声称）、`移植stm32实机记录.md`（新节，节号实测取）、`BACKLOG.md`（REQ-7 状态 + REQ-10…18，规划者预改）、`ROADMAP.md`（M4 + M3 收尾，P0-6）、`versions/v2.29/` 三件套、`新功能提案.md`（入库，内容零改动）。
  - **登记行 carve-out**（沿 v2.28 计划 §1.3/HC-1 已立条文）：P4-3 对 `v2.20`~`v2.28` 各 OK 档 +1 token 行属 docref (d) 桶机制强制面，**不计偏差**；判据 = 各档 `1+/0−` 纯登记行。
  - 库外：`D:\code\STM32CubeMX\G474VET6_ET_TEST\**`（分区配置 / 链接脚本 / 私有 demo / 证据目录 `build\v2.29-evidence\`）。
- **HC-2 git 锚定与触发面**：基线 = tag `v2.28`（HC-0④ 定着后）；快照 `snap-2.29-start`（**无 `v` 前缀**）打于入仓批末笔，打后**实测触发面**（`gh run list --limit 3` / `gh release list`，读数入交付）；分支 `dev/v2.29`；发版 tag `v2.29` 在全部改动定着 `main` 之后。
- **HC-3 证据面**：⑧ 机读值 = `sh tools/gatecheck.sh 2.29 --emit` 逐字粘贴；`--verify` 绿态 = 定稿链末笔；封闭枚举四类复测留痕 `build/final-gates-v229.log`（本版起**留痕件段数 = 声称面**，CO-11 教训）。**容量面证据**：REQ-7 的分区表与体量算式须有实测读数（`arm-none-eabi-size` 产物 + 分区容量来源），禁只写设计值。
- **HC-4 修订登记放行**：本版对已发布档的实质变更面 = 登记行 carve-out（纯登记）；`versions/v2.25/`~`versions/v2.28/` 内容零触碰。若必须触碰，走 HC-4 放行 + 偏差登记。
- **HC-5 数字回刷同批（门强制面 ⊆ 本清单，逐值标注口径）**：MINOR 29 / `0x021D00`；README 版本行 + 交付链接行 + 基线名前滚 ×2（→ v2.28 归档）；CHANGELOG `[2.29.0]` 日期 = 终态提交日；architecture 机制门行 = N（P4-6 新公式预置 + 终态实测）；**例数 5→6**（`examples/ex_min.c` 新增 ⇒ `docsync.sh:476-487` 五处一致面 + `README.md:191` + `docs/architecture.md:113` 同批，禁静默）；**用例数**（现值 520 系，四形态 520/520/527/521 @v2.28 终态）随 region/bootctl 新用例滚动，回刷面 = `grep -rn` 终态全列点名；模块数 **35 维持**、开关数 **32 维持**；两 port README v2.29 体积行以 `arm-none-eabi-size` 构建实测回填；`API_INVENTORY.md` 重生成（新增 port API + bootctl 字段面）。
- **HC-6 契约零破坏**：`apidump --check` exit 0；`apidump --diff` 对 v2.28 基线 = **修改/删除 0 项，新增 = 本版点名清单**（`port_flash_region_*` 与常量 + `et_bootctl_cfg_t` 追加字段；本版为**追加型**，判据不得写 0/0/0 全零）；`et_bootctl_init/verify_image/stage/confirm/should_rollback/state/abandon` 七签名逐字不变；**`port_flash_read/write/erase_sector` 三签名与语义逐字不变**（区域 API 独立新增）；`API_STABILITY.md` §5.1 追加行 = 1（`et_bootctl_cfg_t`）。
- **HC-7 门改动边界（纯新增，判定语义零变更）**：`docsync.sh` 允许面 = ① P1-2 **CMake↔Makefile 源列表一致性**断言（防双源漂移；判据 = 七目录内每个 `.c` 必须出现在 Makefile 对应 `*_SRC` 变量中，CMake 侧用 glob `CONFIGURE_DEPENDS` 自动收集）；② P3-1 **并发契约表行数 = 模块数**断言；③ P4 数值字面。同批 architecture 回填 + N 复算 + docref 登记。
- **HC-8 如实性与口径**：① **N-01 的「关掉的模块不编译」按「不产生目标代码与符号」口径实现并在支持矩阵/文档写明**（CMake 以 `-DET_MODULE_X=0` 编译空 TU；`nm`/map 零符号；禁写"源文件不参与编译"这类未实测断言）；② 支持矩阵每一「验证过」行须附可复现命令或 CI job 名，**未跑过的一律「未验证/不支持」**；③ 板侧读数禁伪造（干跑 ≠ 板侧绿）；④ 计数类逐值标注口径（numstat / 行计数 / 出现次数 / 文件面数 / 列数），不混用；⑤ 评审/交付的时点声称标「取数时点 SHA + 落笔时点 SHA」（CO-10 条文）。
- **HC-9 分层与不变式**：区域 API 属 `port/` 契约面（唯一碰硬件层）；`core/algorithm` 零 port.h 不变式保持；`storage/et_bootctl` 不得反向依赖上层；host port 必须实现 region API（host 可测性）；f103 若不实现须以编译期宏显式声明「不支持区域 API」并由 bootctl 拒绝该路径（不静默降级）；零动态内存、多实例、ISR 标注不变。
- **HC-10 板侧义务（REQ-7 专项，容量维必填）**：① 分区设计须给**算式 + 实测**：仓库实测 G474 flash = **512 KB / 页 2 KB**（`port/stm32g474/port_stm32g474.c:91-92`），参数区尾 **16 页 = 32 KB**（`:89-90/:49-54`），当版真固件 FLASH = **48,140 B**（v2.28 `BLOCKED-2` 实测）⇒ 槽规划须 ≥ 64 KB × 2 + 状态页 + 参数区 32 KB，总占用 ≤ 512 KB；AC 须写「产物 ≤ 槽容量」的实测比较（禁只写设计值）；② 镜像 body = 当版真固件；`--ver` 奇偶只切 demo 自检路径（与 MINOR 无关，沿 v2.28 HC-10③）；③ confirm 单判据 = `AT+BOOTINFO` 三字段、回滚单须证明 A 槽存活且 `attempts` 递增；④ 烧前版本串机检不过不得烧录；⑤ 板侧红 = 真实发现走 BLOCKED，不改库面绕过。

## 3. 任务分解

### P0 采纳前置与条文批（G0 窗口，零库面）

- **P0-1（REQ-18/前置1.1）`LICENSE`（MIT）**：仓库根新增 MIT 全文，版权行 = `Copyright (c) 2026 azura`（`git config user.name` 实测；若 Owner 要求实名，批准时替换）。判据 = 文件在位 + `grep -c "MIT License"` = 1 + 版权行命中 + README 增加「许可证」链接行。来源 = `REQ-18` + Owner 2026-10-20 裁决。
- **P0-2（REQ-18/前置1.2）支持矩阵**：`README.md` 新节「支持矩阵」+ 链接入 `docs/getting-started.md`。列 = 维度 / 状态（验证过 / 未验证 / 不支持）/ 验证方式。行至少覆盖：C99（验证过，`make WERROR=1 test`）、C11（未验证）、C++ 混编（未验证，`extern "C"` 已备）、host gcc MinGW（验证过，本地门）、host gcc Linux（验证过，CI `unit-tests/ubuntu`）、clang（按 CI/本地实测如实写）、ARM GNU 13.3.rel1（验证过，`cross f103/g474` job + docbuild）、IAR/ARMCC（不支持/未验证，明确写）、STM32F103C8T6（真机验证 + Renode smoke）、STM32G474VET6（真机验证）、ch32x035（仅编译通过，上板回归挂账 —— 触发 = WCH-Link 枚举）、FreeRTOS（未验证，本版只出契约表）、其他 RTOS（不支持）。判据 = 表格行数 ≥ 12 且每「验证过」行含命令/CI job 名；`docsync` 断言该节存在（纯新增）。来源 = `REQ-18` + 常设（README 清单⑥ 环境注记）。
- **P0-3（REQ-18/前置1.3）10 分钟样例**：新增 `examples/ex_min.c`（ringbuf + kv 最小可运行，含预期输出注释）→ **同批回刷例数面**（`docsync.sh:476-487` 五处一致 + `README.md:191` + `docs/architecture.md:113`）；`docs/getting-started.md` 增「§0.5 十分钟最小样机」一页纸（完整命令 / 预期输出 / 常见失败：编译器版本、字符集、路径空格；环境注记精确到工具链版本）。判据 = 干净命令序列可复现（`make ex` 或 `cmake` 两条路径各一条）+ 例数面同值（五处 = 6）。来源 = `REQ-18`。
- **P0-4（CO-7/CO-10/RC-4/RC-5）四条模板条文**：① `docs/开发计划模板.md` §7④ 增**「体量 vs 容量」必填维**（写算式 + 实测读数 + 容量来源；反例 = v2.28 `BLOCKED-2`）；② 两模板增**评审/交付时点三要素**（落笔前重取 HEAD/⑧ 态/区间；标「取数时点 SHA + 落笔时点 SHA」）；③ 两模板增**申报件白名单归属**（RC-3：BLOCKED/申报件不得靠文件名巧合命中白名单；判定语义变化随 v3 池）；④ `docs/开发计划模板.md` 交付面条文增**末笔身份单点承载**（围栏首行为唯一权威，其余处只写「见 §4 首行」）+ **1:1 补充笔上限**（超出即转 r3/复评）+ **DELIVERY §8「新增破坏性候选」段落点义务**（RC-4）。判据 = 五处各 `grep -c` = 1 且**纯追加**（旧行为新行严格前缀，程序比对）。来源 = `CO-7(v2.28)` / `CO-10(v2.28)` / `RC-3` / `RC-4` / `RC-5`。
- **P0-5（RC-3）申报件归属条文**：并入 P0-4③（落点同批，此处仅登记判据：`tools/docref.sh` 头注只加一句说明或模板承载，**不动判定语义**）。来源 = `RC-3`。
- **P0-6（M4）ROADMAP 登记 + M3 收尾**：`ROADMAP.md` 新建 **M4「首次接入与产品化路径」**（窗口 v2.29~v2.32；判据 = ① 采纳前置三件在位 / ② N-01 CMake 接入可验证 / ③ 并发契约表在位 / ④ 真 A/B 分区与双单走单）；N-03/04/05 列 M4 后续候选，N-06/07/08 未排期；**M3 收尾**：⑤ 按 v2.28 HC-0① 的实际结局定着（成功 → 达成归档；未成功 → 转 M4 关联并如实登记），M3 ①②③④ 注记随 v2.28 复评结论改写。判据 = `^\*\*M4` 行 = 1 + M3 段含收尾状态行。来源 = `M-4` + Owner 裁决。
- **G0 窗口判据**（右端 = G0 末笔 SHA 实测回填）：窗口谓词逐文件枚举既有门（`docref.sh` / `gatecheck.sh` / `sizecheck.sh`）+ 六库目录 + `et_config.h` + `Makefile` = diff **空**；`docsync.sh` 显式排除并归因（P1-2/P3-1 计划内）；本批触碰面（`LICENSE` / 两模板 / `README.md` / `getting-started.md` / `examples/ex_min.c` / `ROADMAP.md` / `BACKLOG.md`）实测非空并写明归因；docsync G0 终态 = 复算值。

### P1 N-01 CMake 接入（REQ-10）

- **P1-1 根 `CMakeLists.txt` + `cmake/` 导出**：目标 `et::embedded_tools`；源码 = 七目录 `file(GLOB CONFIGURE_DEPENDS *.c)` 自动收集（与 port/CI 既有 glob 口径一致）；模块开关 = CMake option → `target_compile_definitions(ET_MODULE_X=0/1)`（**默认值取 `et_config.h`**，option 默认 = ON 表示不覆盖）；include 目录 = 根（`et_config.h` + `port/port.h`）+ 七目录；host 目标 + 交叉目标由 toolchain file 决定（不写死工具链）；`install()` + `export()` + `find_package(et CONFIG)`；C99 强制（`C_STANDARD 99`）。判据 = 空工程 5~10 行接入可编可跑（AC-3）。来源 = `REQ-10`。
- **P1-2 双源一致性机检 + CI**：`tools/docsync.sh` 增**纯新增**断言——七目录每个 `.c` 必须出现在 Makefile 对应 `CORE_SRC/ALGO_SRC/SYS_SRC/PROTO_SRC/DRIVERS_SRC/DEBUG_SRC/STORAGE_SRC` 变量中（缺失即红，防「CMake glob 收进来了但 host 测试没跑」）；`.github/workflows/ci.yml` 增 CMake 构建 job（ubuntu，`cmake -B build -DET_MODULE_X=0` 变体 + `cmake --install` + 一个 `find_package` 消费样例；**不得**替代既有 Makefile job）。判据 = 断言在位 + 反向探针（临时树移除一个源文件行 → 红）+ CI 新 job 绿。来源 = `REQ-10` + 常设（README 清单①「无断言的同步声明视为未同步」）。
- **P1-3 接入文档**：`docs/getting-started.md` 增「CMake 接入」小节（`add_subdirectory` 与 `find_package` 两形态、模块 option 表、交叉编译 toolchain 示例、常见失败）；`README.md` 构建面增一行指向。判据 = 两条命令序列可复现。来源 = `REQ-10`。

### P2 REQ-7 真 A/B 分区（契约面）

- **P2-1 port 区域访问契约（纯追加）**：`port/port.h` 增**可选区域 API**（语义提案，实现细节可裁量）：区域枚举（参数区 / 槽 A / 槽 B / 状态页）+ 区域内 read/write/erase + 区域容量查询；既有 `port_flash_*` 三函数**逐字不变**；未实现区域 API 的 port 以编译期宏声明（f103 若不做 → 显式「不支持」，bootctl 拒绝区域路径）。**容量算式（起草期实测输入）**：flash 512 KB / 页 2 KB（`port_stm32g474/port_stm32g474.c:91-92`）；参数区尾 16 页 = 32 KB；当版真固件 48,140 B ⇒ 规划槽 ≥ **64 KB × 2** + 状态页 2 KB + 参数区 32 KB = 162 KB ≤ 512 KB（余量 350 KB）。判据 = 头文件新增面在 `apidump --diff` 点名清单内 + 三既有签名逐字不变。来源 = `REQ-7` / `CO-7(v2.28)`。
- **P2-2 host 实现 + 单测**：`port/host/port_host.c` 实现多区域模拟（含断电/坏块注入沿用既有 host 机制）+ `test/test_bootctl.c` 增：跨区域 stage/rollback、槽容量校验（`img_size > slot_size` 拒绝）、区域不支持路径。判据 = `make test` 全绿 + 新用例可独立复现。来源 = `REQ-7` + 常设（不变式 host 可测）。
- **P2-3 G474 私有工程分区（库外）**：`D:\code\STM32CubeMX\G474VET6_ET_TEST\` 内定义真分区（A 槽应用区 / B 槽 64 KB / 状态页 / 参数区尾 32 KB），链接脚本与私有 demo 配置同批；`et_bootctl_cfg_t` 尾部追加承载槽基址（字段语义：0 = 旧 `slot_sector` 语义，非 0 = 绝对基址）并写 `docs/API_STABILITY.md` §5.1 追加行。判据 = 分区表读数（链接脚本/CFG 实读）+ `diff_rq` 同步留痕。来源 = `REQ-7`。
- **P2-4 真固件双单走单**：板侧构建当版真固件（产物实测体量 ≤ 槽容量，AC 写实测比较）→ `pack_image.py --ver` 奇/偶各一份 → `xmodem_send.py` 真传输 → **confirm 单**（`AT+BOOTINFO` 三字段）→ **回滚单**（A 槽存活 + `attempts` 递增）；证据落 `build\v2.29-evidence\`；实机记录新节。判据 = AC-8。来源 = `REQ-7` + 常设（模板 §2 真机证据）。

### P3 并发契约表与工具修复

- **P3-1（REQ-11）`docs/concurrency.md`**：35 模块 × 4 列（ISR 可调用 / 任务可调用 / 单所有者 / 多生产者是否安全）+ 每行证据 = 头文件既有标注位置（`🔒ISR-safe` / `🏠MAIN` 双标形态，v2.27 起的标注面）；顶部写明「本表是标注的汇总，不新增行为承诺；FreeRTOS port 未验证」。`docsync.sh` 增断言：表行数 = 模块数 35（防漏行）。判据 = 行数 = 35 + 每行证据可 grep 到 + 无与头注相反的条目（抽检 ≥5 行）。来源 = `REQ-11` + 常设（README 清单①）。
- **P3-2（RC-1）`board_window.py` 串口异常修复**：`walk()` 与连接面捕获 `serial.SerialException`/`PermissionError` → 归入 `abort_reason` + 保留已生成件 + 退出码区分（机检失败 vs 端口不可用）；**不得**把异常捕获写成"重试直到成功"（保留真实失败证据）。判据 = 假端口/占用端口注入实测：不 traceback、证据件在档、退出码语义在 `--help` 与文档一致。来源 = `RC-1`。

### P4 常设批与收口

- **P4-1 基线滚动三件事**（常设，模板 §7⑥）：① `docs/API_INVENTORY_v2.28.md` 归档（`git worktree` 于 tag v2.28 真生成 + blob sha256 + 与 v2.27 归档 diff 逐段；v2.28 公开面零新增 ⇒ 预期仅版本宏行，实测登记）；② `tools/apidump.sh` BASE 切 v2.28（**实测两处**）；③ 发版点复读 = 修改/删除 0、新增 = 点名清单（HC-6）。
- **P4-2 数字回刷同批**（HC-5 全清单，分两段）：**(a) 版本宏段必须先于 P2-4 烧录**（板上 `AT+VER` = `v2.29.0`）；**(b) 其余数值段随定稿批**（例数 5→6 全列、用例数全列、门两处、architecture 机制门行 N、两 port 体积行实测、`API_INVENTORY.md` 重生成）。
- **P4-3 登记链收口**：以 docref **实测点名面**为准追加登记 token（本版触碰 `docsync.sh` / `port/` / `storage/et_bootctl` / `board_window.py` / 两模板 / 新增 `CMakeLists.txt`），形态 = 纯登记行（HC-1 carve-out）；(d) 桶分类值以发版点机读为准。
- **P4-4 `versions/v2.29/DELIVERY.md`**：沿权威模板 §1-§8 + 行首声明「本版 docsync：N/N」（终态实测）+ 覆盖率行（含「CMake 双路径 / 板侧真分区」两面）+ §4 记录范围（`snap-2.29-start..` + 右端时点化）+ §7.4⑧ 逐字片段 + §7.5 复现预期 + **§8「新增破坏性候选」段必在**（无候选写「无」，RC-4）+ 同批登记面三件（「v2.29 复评记录」节先测节号 / G474+ch32x035+f103 v2.29 行 / §6 零裸占位符）。
- **P4-5 定稿链与发版**：定稿 → `--emit` 逐字 → ⑧ 四类复测留痕（段数 = 声称面）→ `--verify` PASS @定稿链末笔 → tag `v2.29` → CI 七 job + Release 四附件 → tag 后 verify 红 = 预期态 → 删 `dev/v2.29`、tag 对齐。若产生 rN 档：先读 CO-8(v2.26) 与 v2.27 P0-4／v2.28 RC-5 条文（末笔身份单点承载；`head -1` 定位切换预期态）。
- **P4-6 N 公式**：沿用 `CO-10(v2.28)` 修正后的公式（占位符拦截面项），本版预置见文末合规声明。

## 4. 验收标准（全量档；证据落点 = `versions/v2.29/DELIVERY.md` AC 自验表 + `移植stm32实机记录.md` 新节 + `build/` 留痕 + 库外证据目录）

- **AC-0（HC-0 前置）**：交付首行登记五项实测：真板走单复跑读数（五要素对照表）、CO-8 三择一裁决原文、`git tag v2.28` 存在且 `^{}` = `ef339dd`、`main` 含 `dev/v2.28`（`merge-base --is-ancestor`）、CI/Release 读数；任一缺失 = 未开工合法性（BLOCKED 件号在案）。
- **AC-1（P0-1）**：`LICENSE` 在位且为 MIT 全文，版权行 = 实测 `user.name`；README 含许可证链接。
- **AC-2（P0-2）**：支持矩阵在位（行 ≥ 12），每「验证过」行含可复现命令或 CI job 名，每「未验证/不支持」行无"疑似支持"措辞；`docsync` 断言该节存在；抽检 2 行独立复跑。
- **AC-3（P0-3）**：`examples/ex_min.c` 存在且 `make ex`/CMake 两条路径各能跑出预期输出；getting-started 一页纸含完整命令/预期输出/常见失败三类；**例数面五处同值 = 6**（`grep` 实测，禁残 5）。
- **AC-4（P0-4/P0-5/RC-4/RC-5）**：五处条文各 `grep -c` = 1 且纯追加（旧行严格前缀程序比对）；DELIVERY §8 段落在位（RC-4）；末笔身份单点承载条文在位。
- **AC-5（P0-6 / M4）**：`ROADMAP.md` `^\*\*M4` = 1，四条判据逐条可指到本版交付物；M3 收尾状态行在位（⑤ 结局与 v2.28 HC-0① 实测一致）。
- **AC-6（P1-1 / REQ-10）**：空 CMake 工程 **5~10 行**接入（`add_subdirectory` 与 `find_package` 各一形态）可编可跑 ringbuf+KV host demo；ARM toolchain 交叉编译 **0 warning**（命令与输出在档）；`-DET_MODULE_KV=0` 后 `nm`/map **无 KV 符号**（实测命令在档，口径 = 无目标代码/符号，HC-8①）；`cmake --install` 后另一工程 `find_package(et CONFIG)` 成功；接入代码在 MINOR 升级位形下零修改（以本版内两次配置构建证明形态，禁跨版虚构）。
- **AC-7（P1-2）**：docsync 双源一致性断言在位；反向探针（临时树移除一条 Makefile 源列表行 → 该断言点名 FAIL，`git archive` 临时树执行、主树零写入）；CI 新 CMake job 绿且不替代既有 job；`.github/workflows/ci.yml` diff 仅新增 job 段。
- **AC-8（P2 / REQ-7）**：分区表实测（A/B/状态/参数区地址与容量，来源 = 链接脚本/CFG 实读）；当版真固件产物体量实测 ≤ 槽容量（**算式 + 实测双读数**）；host 单测（跨区域 stage/rollback/超容拒绝）全绿；真固件双单证据在档（confirm：`AT+BOOTINFO` 三字段 + `AT+VER` 当版；回滚：A 槽存活 + `attempts` 递增）；`--ver` 奇偶参数逐处标注（禁"版本号导致回滚"表述）；`apidump --diff` 新增项与点名清单一致、修改/删除 0；`API_STABILITY` §5.1 追加行 = 1；f103「不支持区域 API」声明在位（若适用）。
- **AC-9（P3-1 / REQ-11）**：`docs/concurrency.md` 行数 = 35（机检断言）；抽检 ≥5 行与头注一致；无「FreeRTOS 已验证」类表述。
- **AC-10（P3-2 / RC-1）**：占用端口注入实测 —— 无 traceback、`abort_reason` 与证据件在位、退出码语义与文档一致；正常路径行为不变（假板 `check-only` exit 0）。
- **AC-11（P4-1）**：`API_INVENTORY_v2.28.md` 存在 + blob sha256 + 与 v2.27 归档 diff 逐段；`apidump.sh` BASE 两处实测；发版点复读形态正确。
- **AC-12（P4-2）**：MINOR 29 / `0x021D00`（`gcc -E -dM` 复现）；README 版本/链接/基线名；例数 6 五处同值；用例数回刷面 `grep -rn` 终态全列同值；模块数 35 / 开关数 32；CHANGELOG `[2.29.0]` 日期 = 终态提交日；体积行构建实测。
- **AC-13（全量回归与发布面）**：四形态读数实测（基线 520/520/527/521）+ `make ex` **六**配方 PASS；`docsync` = N/N fail=0；`apidump --check` exit 0；`sizecheck` 3/0；`docref` 逐档点名清单（(d) 逐文档，禁聚合行）；CI 七 job（含新 CMake job）+ Release Latest 四附件 + 快照触发面实测；`make WERROR=1 test` 与 `make test-asan`（若 HC-0② 裁决要求本机跑则如实登记可达性）。
- **AC-14（⑧ 与 (d) 全集）**：⑧ 五锚行 = `--emit` 逐字（`diff` 空）；`--verify` PASS @定稿链末笔；tag 后红 = 预期态与 §7.5 逐字吻合；(d) 全集 = 35 + 1 = **36 份**（v2.28-r2 终态 35 份 + 本版 DELIVERY；rN 预期 0；以发版点机读为准）；token 机读与 §7.3 一致。
- **AC-15（G0 窗口）**：窗口谓词（§3 P0 逐文件枚举面）于 G0 终态提交实测 = 空；`docsync.sh` 排除归因 + 触碰面归因在案；六库目录 + `Makefile` + 其余三门零 diff。

## 5. 可裁量项与不可偏离

- **可裁量**：① 区域 API 的具体形态（区域 id 枚举 vs 描述符数组；`port_flash_region_*` 命名）；② CMake option 命名与默认值表达（ON 表示不覆盖的语义须文档化）；③ `ex_min.c` 的模块组合与输出格式（须 ringbuf + kv）；④ 支持矩阵的列裁剪（维度不可少，列可合并）；⑤ 并发契约表的 4 列实现（列不可少，措辞/排序可裁量）；⑥ `board_window.py` 异常分类粒度；⑦ 提交切分粒度与 (d) 点名面（以实测为准）；⑧ DELIVERY 章节编排（沿模板）。
- **不可偏离**：§1.1 全部处置结论（尤其 REQ-11 只做契约表、N-03/04/05/06/07/08 不认领）；§2 全部 HC——尤其 **HC-0**（前置未定着即 BLOCKED）、**HC-6**（port 三签名与 bootctl 七签名逐字不变）、**HC-8①②**（"不编译"口径与支持矩阵不得夸大）、**HC-10**（容量算式 + 实测）；§1.3 Non-goals（不新增开关/套件、不改门判定语义、不动两板工程本体、不代做发版）；§6 基线规则；历史文档冻结面（除登记行 carve-out）。

## 6. 基线

| 字段 | 内容 |
|---|---|
| 首选基线 | **`tag v2.28`**——起草时点**不存在**（实测 `git tag --list "v2.2*"` 止于 v2.27）。HC-0④ 定着后由 B1 `git rev-parse v2.28^{}` 实测回填 |
| **开工前置（HC-0）** | ① 真板走单复跑（M3-⑤）；② CO-8 三择一；③ 合 `dev/v2.28`（`ef339dd`）入 `main`；④ tag `v2.28` 落 `ef339dd`；⑤ CI + Release + `dev/*` 清空。均为 Owner/B 动作，未完成 = BLOCKED，不得改用 SHA 锚点偷跑 |
| 入仓批（预申报） | `新功能提案.md`（用户原件，内容零改动）+ `BACKLOG.md`/`ROADMAP.md` 规划者预改（REQ-10…18、M4）+ 本计划；`snap-2.29-start` 打于该批末笔并实测触发面（HC-2） |
| 快照 | `git tag snap-2.29-start <入仓批末笔>`（**无 `v` 前缀**） |
| 分支规则 | 改动在 `dev/v2.29`；发版 tag `v2.29` 在全部定着 `main` 之后；`dev/*` 残留本版收口确认清空 |
| **顺序规则** | ① HC-0 → ② 入仓批 + 快照 → ③ P0（G0，零库面）→ ④ P1 CMake（含 CI job 与一致性断言）→ ⑤ P2-1/P2-2 库面契约与 host 测试 → ⑥ P4-2(a) 版本宏回刷 → ⑦ P2-3/P2-4 板侧分区与双单 → ⑧ P3 契约表与工具修复 → ⑨ P4-1/P4-2(b)/P4-3/P4-4 → ⑩ P4-5 定稿链与发版。**板不可达时**：⑦ 转 BLOCKED + 挂账续期（G474 条目 v2.29 行），其余项可先行；**禁止**以干跑/host 模拟冒充板侧绿 |
| 偏离申报 | 实际起点 = 入仓批末笔（≠ tag v2.28）属预申报形态，交付首行如实声明；HC-0 实测值缺失 = 未达开工条件，不计偏差 |

## 7. 起草期核验清单（六项全过）

| # | 检查项 | 本计划执行情况 |
|---|---|---|
| ① | 内部 tag 命名 | `snap-2.29-start` 无 v 前缀；触发面实测义务入 HC-2；软重置检查随 B |
| ② | "无副作用/不会触发"类断言逐条实测 | 起草期实测（2026-10-20，A1 对话）：`git tag --list "v2.2*"` 止于 **v2.27**；`main` = `5d33d2f`；`dev/v2.28` = `ef339dd`，`main..HEAD` = **35**、`HEAD..main` = 0；`git status` = 仅 `?? .qoder-credits/`、`?? 新功能提案.md`；`sh tools/docsync.sh` = **378/0**（本机实跑；架构门行 `docs/architecture.md:114` 同值）；`LICENSE*`/`COPYING*` = 不存在；根 `CMakeLists.txt` = 不存在；C99 声明面 `README.md:3`/`getting-started.md:4`；`README.md:172` 仅一行「已验证平台」；FreeRTOS 全库近乎零提及；**G474 容量面**：`port/stm32g474/port_stm32g474.c:91-92` flash 512 KB / 页 2 KB、`:89-90/:49-54` 参数区尾 16 页 = 32 KB、v2.28 `BLOCKED-2` 当版真固件 48,140 B；`PORT_FLASH` 三签名 `port/port.h:67-69` 实读；`et_bootctl_cfg_t` 四字段实读（`state_sector`/`slot_sector[2]`/`slot_size`/`max_attempts`）；例数 = 5（`ls examples/ex_*.c`）；`520` 字面 13 处；开关 32；`versions/v2.28/` 四件（缺 `REVIEW-r2.md`）；r2 清偿抽检：`board_window.py:154/:162/:193-197` 残差分列、`docs/v3-candidates.md:629` 池节、`README.md:5` 链接、`build/final-gates-v228.log` 15,132 B。转交付/终态实测：N、四形态、体积、⑧ 锚值、板侧全部读数、CMake 双路径实测 |
| ③ | 板侧落点标注 | 私有文件全路径：`D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\Src\et_demo.c`、同步目录 `...\Core\et\`、分区配置/链接脚本（同工程内，P2-3 实测登记具体文件）、真固件产物 `...\build\Release\g474.bin`（v2.28 实测在位）、证据 `...\build\v2.29-evidence\`；**禁止**写成库内 `examples/`；库内 `examples/ex_min.c` 为 host 样例，不参与板侧同步 |
| ④ | AC ↔ 冻结 API 形状互洽 | 逐条核过：① **REQ-7 容量维**（本版教训固化）：槽 ≥ 64 KB × 2 + 状态 2 KB + 参数区 32 KB vs 真固件 48,140 B ⇒ 算式成立且 AC 要求实测比较（模板 §7④ 新条的首个实例）；② `et_bootctl_cfg_t` 尾部追加 vs `apidump` 不可见 ⇒ `API_STABILITY` §5.1 人工登记行（AC-8）；③ port 三既有签名不变 vs AC 断言「逐字不变」可同时成立（区域 API 独立命名）；④ N-01「关模块无符号」断言对象 = `nm`/map（与 `#if` 空 TU 实现一致，HC-8① 口径写明）；⑤ 本版 `--diff` 判据 = 新增点名清单（非全零），与追加型实现一致；⑥ 例数 5→6 与 `docsync:476-487` 五处一致面同批 |
| ⑤ | 证据落点章节号 + 占位节清理 | 板侧证据落点 = `移植stm32实机记录.md` 新节——落笔前 `grep -n '^## '` 实测取号（v2.28 后现至 §18 预期，禁写死）；`docs/concurrency.md` 为新建独立文档（无节号冲突）；「v2.29 复评记录」节落笔前 `grep -n '^## '` 复核（追加即复测）；本版不取代旧占位节 |
| ⑥ | 基线滚动三件事 | P4-1 三件：归档 v2.28（worktree 于 tag v2.28 真生成 + sha256 + 与 v2.27 归档 diff 逐段）/ BASE 切 v2.28（**实测两处**）/ 发版点复读（修改删除 0 + 新增点名清单） |

---

**起草期合规声明**：六项已于起草对话（2026-10-20）执行，实测命令与结果全在 §7②（含 docsync 378/0 本机实跑、tag/main/分支态、LICENSE/CMake/FreeRTOS/C99/支持矩阵现状、G474 512 KB/2 KB/32 KB 与 48,140 B 容量面、r2 清偿抽检、例数与 520 字面面）。**规划产出件写出后复跑同值**：本计划与 BACKLOG/ROADMAP 预改落盘后 `sh tools/docsync.sh` 复跑 = **378/0**（规划者不扰动门）。

**预置数值按值声明与逐项复算（沿用 CO-10(v2.28) 修正后的 N 公式，自证）**：
- docsync N 预置 = 上版终态实测 **378** + **1**（本版 `DELIVERY.md` 覆盖率行入循环）+ **0**（本版 rN 文件数，预期）+ **1**（CHANGELOG `[2.29.0]` 链）+ **K**（本版新增断言：P1-2 双源一致性 + P3-1 契约表行数 + P0-2 支持矩阵节存在性，预期 3，以 diff 实测）− **1**（上版 rN 档 1 份的占位符拦截面退场）= **379 + K**；**N 以终态实测回填，禁预写**。
- 用例数基线 = **520**（四形态 520/520/527/521 @v2.28 终态）；本版新增 = region/bootctl 用例，增量以 `make test` 终态实测；回刷面 = `grep -rn` 终态全列点名。
- 例数 = **5→6**（`examples/ex_min.c`），五处一致面同批（`docsync:476-487` + README:191 + architecture:113）；模块数 **35**、开关数 **32** 维持；板上套件 **22** 维持。
- 体积行 = `arm-none-eabi-size` 构建实测回填（区域 API + bootctl 追加致 .text 微增，禁沿用 v2.28 值：f103 25600/24/1512 与 38044/24/3792、g474 25976/28/1512 —— 以上为 v2.27 值，v2.28 另有实测值，**以本版构建为准**）。
- (d) 全集 = 35（v2.28-r2 终态）+ 1 = **36 份**，分类值以发版点 `--emit` 机读为准。
- 版本宏 = MINOR **29** / hex `0x021D00`（现值 28 / `0x021C00` 实测）；基线 `tag v2.28` = 待 HC-0 定着后实测（起草时点不存在，如实登记）。
- 容量面 = flash 512 KB / 页 2 KB / 当版真固件 48,140 B / 槽规划 ≥ 64 KB×2（余量 17,396 B 单槽 ≈ 26.5%）——**P2-1 的算式与 AC-8 的实测比较均以此为准，实测偏离即登记**。

**纪律自查**：判据区间右端时点化（G0 末笔 / 定稿链末笔 / P4-2 分段前置清单封闭）；G0 窗口谓词逐文件枚举既有门、禁裸目录；含「修订登记」短语行不携带 token（分行形态）；引用失实原文一律入 code span 或围栏；机读值 `--emit` 逐字 + `--verify` 定稿链末笔；计数逐值标注口径；**工具/契约改动一律写实测行数与点名清单**（CO-1(v2.27) 机制）；**体量/容量算式必附实测**（CO-7(v2.28) 新条）；评审/交付声称标「取数时点 SHA + 落笔时点 SHA」（CO-10(v2.28)）；末笔身份单点承载（RC-5），1:1 补充笔设上限。

## 变更记录（冻结后只允许追加，需 Owner 批准）

> 追加式登记。**同日各笔的实际时序以行内事件为据**（初稿冻结 → 发版推送 → 走单复跑 11:06 → clang ASan 实测 12:10），本表行的物理次序不主张时间序。

| 日期 | 变更 | 原因 | 批准人 |
|---|---|---|---|
| 2026-10-20 | 初稿并即冻结（规划者 A1 对话产出）：四路盘点 → 出口 A「接入版」全量档；§1.1 行 19-25 为 `新功能提案.md` 逐条映射（REQ-10…REQ-18）；HC-0 承载 v2.28 未闭合与发版前置；BACKLOG/ROADMAP 规划者预改（REQ 状态 + M4/M3 收尾） | Owner 四裁决（接入版认领 / MIT / 先补 v2.28 发版 / 新建 M4）；批准依据 = standing 规则「规划下一版 = 默认批准」 | Owner（2026-10-20） |
| 2026-10-20 | **`CO-8(v2.28)-P` 裁决落定 = 路径①'（本机 clang ASan 可达），§1.1 行 8 的「暂缓」改「纳入」，新增 P3-3** | Owner 提示「本地已装 clang 最新版」后规划者本机实测（全部为只读探针 + 门实跑，主树零 tracked 改动、`build/` 为 gitignore 物证）：clang **23.1.0**（`D:/software/LLVM/bin/clang`，target `x86_64-pc-windows-msvc`）；ASan 运行时件在位 = `D:/software/LLVM/lib/clang/23/lib/windows/clang_rt.asan_dynamic-x86_64.dll`；**探针实证** = TEMP 内故意 use-after-free → `AddressSanitizer: heap-use-after-free`、RC=1（此前 rc=127 的成因 = DLL 不在 PATH，加上即通）；**仓库门实证** = `mingw32-make test-asan CC=clang` 与 `test-asan-1k CC=clang` 均 `=== RESULT: ALL PASS (fail=0) ===`、MAKE_RC=**0**（含 `debug/metrics pass=16`、`storage/bc pass=24`）；**对照面仍不成立** = `rm -f build/et_tests_asan.exe` 后裸 `make test-asan`（CC=gcc MinGW 16.1.0）= `cannot find -lasan`、rc=2 ⇒ 「本机不可执行」的历史结论只对 gcc 与 clang 17 成立（README:219 / Makefile:104-106/:116-117，v2.5 D-2、v2.6 CO-13）。**取证自记**：首次「gcc 仍不可执行」的对照我用了未清理的产物状态跑 make，得到 0 命中的假象（目标 up-to-date 直接跳过编译）——`rm` 后才是真判据，纪律 = 对照实验必先删产物。**新增 P3-3（来源 = `CO-8(v2.28)-P`）判据四项**：① 一条命令本地可复现 ASan 门（不依赖手工 `export PATH`——由 Makefile 变量或拷贝运行时 DLL 承担，形态可裁量）；② `README.md:219` 与 Makefile 两处注释的「本机不可执行」按值勘误为「gcc 不可 / clang 23.1.0 可达（附 PATH 与命令）」；③ 支持矩阵（P0-2）增「内存检查门」行，验证方式 = 实测命令；④ clang 下 **15 条警告**如实登记（10 条 MSVC CRT `strncpy/strcpy` deprecation + 4 条 `set but not used` + 1 条 `self-assign`），零警告门仍以 gcc 为准，clang 警告清或豁免**不在本版义务**（触发 = 下一判定）。HC-0 至此**五项全闭合**，本版可开工——判据来源 = Owner 指示「本地已装 clang 最新版」，实测读数与对照面在档 | Owner（2026-10-20）+ 规划者（登记） |
| 2026-10-20 | **HC-0 进度与 P3-2 判据补强**：③④⑤ 已定着（`main` ff 至 `ef339dd` 并推送、tag `v2.28` = `ef339dd`、CI run `39359483532` success、Release run `39359485783` success → Release **v2.28 Latest 四附件**、本地 `dev/v2.28` 已删）；①（真板走单）经 Owner 指示复跑闭合 = `--mode full` exit 0 且板侧七项读数在档（库外 `build\v2.28-evidence-rerun\`：`ver=2.28.0 boot=50` / `staged=1 confirmed=1 attempts=1` / `FEATURES … sw=32` / `SELFTEST: 22/22` / MBRD 三形态 `disc=0` / `METRICS 11/12 drop=0` / `rx=180 ore=0`；PC 从站 `served=3 crc_err=0 mismatch=0`）⇒ **M3-⑤ 达成**；**P3-2 根因精化**（不改任务号，只加判据）：v2.28 失败态的真实成因是 `walk()` 的 `serial.Serial()` 打开无重试、`run()` 对 `walk()` 无异常护栏、`main()` 直传 `Window.run()` ⇒ 异常以 traceback 逃出并**吞掉 `window_run.log`**（现网证据 = 原 `build\v2.28-evidence\` 有 `build.log/flash.log/version_check.txt/walk_*` 但**无 `window_run.log`**）；`PermissionError(13)` 属烧录后 `-rst` 端口重枚举竞态（瞬态，本次复跑未现）。本版 P3-2 判据相应增两项：**(a) 打开端口有界重试**（次数与间隔实测登记，重试不得掩盖真实占用失败——末次失败仍须落 `abort_reason`）；**(b) 无论走单成否，`window_run.log` 必生成**（含中止原因与已生成件清单）。HC-0 仅剩 ②（`CO-8(v2.28)-P` 内存门三择一） | Owner 指示「先跑走单复跑，看看能不能修好那个 PermissionError」+「继续」；复跑读数与根因取证在档 | Owner（2026-10-20）+ 规划者（登记） |
