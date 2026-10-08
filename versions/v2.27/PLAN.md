# v2.27 开发计划：运行指标注册表、调度失准计数与板侧自描述（功能版）

> 填写：规划者（阶段 2，A1 对话 2026-10-08）。批准依据 = Owner standing 规则「当我让你规划下一个版本时，默认批准」（2026-10-08 本对话明示，同载于项目记忆）→ 交付即冻结。冻结后执行者只打勾；变更只走文末变更记录（需 Owner 批准）。
> 布局：三件套沿 `versions/v2.27/`（v2.25 起新布局）。权威模板 = `docs/开发计划模板.md`；评审侧 = `docs/评审记录模板.md`。
> 板侧同步目录与私有工程文件在**库外**（不在本仓 git），全路径见 §3 P2 与 §7③。

- 状态：**已批准-冻结**（Owner 2026-10-08 判定对话 standing 规则 = 「当我让你规划下一个版本时，默认批准」；起草完成、§7 六项已过、文末合规声明在案。冻结后执行者只打勾，变更只走文末变更记录）
- 判据档位：**全量档（功能版，出口 A）**
- 基线 commit：`074759f`（= tag `v2.26`；起草时点 HEAD = `ec15356`，`git rev-parse` 实测两者不同值——tag 后 r2 批七笔在区间内，实际起点 = 入仓批末笔，预申报见 §6）
- 版本目标：v2.27.0（`ET_VERSION_MINOR` 26→27，hex `0x021B00`，实测 `et_config.h:20/:22` 现状为 26 / `0x021A00`）
- 服务阶段目标：**M3-①②③④**（M3「板上自证与可观测性」由本版 P0-5 登记入 `ROADMAP.md`，草案判据即本版交付项；见 §1.0 末行）

- **价值声明（行为级）**：本版交付后——① 板上敲一条命令即可读出**命名运行指标**（u32 计数/仪表 + 可选直方图挂接），现场排障不再逐模块手写读数路径；② 周期任务**错过周期会被计数**，主循环过载/阻塞在第一手数据上可判（此前只能靠 `last_run` 手推）；③ 板上可自证**烧的是什么版本、哪些模块开关被裁剪**（32 个开关 + 版本三件如实报 0/1），不再只能回工程里对编译配置；④ G474 板侧同步债（实测 `Core/et/et_config.h:20` = MINOR **10**，库内 = **26**，跨 16 版）一次清偿，本轮功能面有三条**真机读数**证据而非仅 host 断言。

## 1. 目标与范围

### 1.0 价值声明与立项判定声明

- **四路盘点（2026-10-08 判定对话实测）**：

| 路 | 结果 |
|---|---|
| ① 用户直连 | 无功能类直连需求；本版判定对话内 Owner 给出三项裁决（认领组合 = 主题批 / G474 板在位可用 / 新建 M3 承接本版）——记为 §1.1 行 15-17 与 §7② 的实测输入，非新增 REQ |
| ② BACKLOG.md | **已接受且未认领 = 5**（2026-10-07 Owner 批准整批接受）：本版**认领 REQ-6（P2）+ REQ-9（P3）+ REQ-8（P3）**三条配套项；**REQ-5（P2）/ REQ-7（P3）本版不认领**（理由见 §1.1 行 16）。REQ-1 已纳入 v2.25 且 r2 评审确认其「条件清偿复评」前置已满足 → 转「已完成」= **Owner 动作**（规划者仅补注记，不改状态位）。REQ-2/3/4 暂缓，外部触发未现（WCH-Link 枚举 / f103 补板 / 第二路独立 UART）。速记区空 |
| ③ 最近一版评审记录 CO 存量 | `versions/v2.26/REVIEW-r2.md`（2026-10-08，结论 = **通过**，0 阻塞项）§5 = 本版 §1.1 全量输入：**CO-M ×4**（CO-6 低-中 / CO-7 低 / CO-8 低-中 / CO-9 低）+ 跨版承接四条 + ch32x035/f103 挂账 + v3 池。CO-1…CO-5(v2.26) 已由 r2 批清偿（本版不重做，仅复核）。**可执行 CO-P = 0**——唯一 CO-P 存量 `CO-8(v2.8)-P`（USART2 双口）触发条件未满足，不可在本版承载；未分级条目按 CO-M 保守处置（本批四条 r2 已分级，无需代判） |
| ④ ROADMAP.md 未达判据 | M1 窗口 v2.25~v2.27 **于本版到期**：① bench 相对比值化已清偿（v2.25 交付 + 评审 AC-5 实证 + r2 保持）；②③④ 外部触发均未现。M2（v3 窗口）触发未现。→ **方向性事件**：Owner 裁决 ②③④ 移出阶段判据、转常设挂账条目，并新建 M3 承接本版（P0-5） |

- **三出口与档位声明**：**出口 A 功能版**——判据 = ≥1 项用户可感差异任务（REQ-6/REQ-8/REQ-9 三条均为板上行为可感，来源 = BACKLOG 已接受 REQ）→ **全量档**（HC 全量含成对证据与自指类，AC 不限数，CO-M 不适用存在性核验豁免）。出口 B 单独不成立并已排除：本轮 CO-M = 4 **< 5** 阈值，唯一 CO-P 触发未现，无板侧数据/安全红线新增。
- **服务阶段目标**：**M3-①②③④**（M3 由 P0-5 登记；判据原文 = 本计划 §1.2 交付项一到一对应，禁"更完善"类表述）。同时本版关闭 M1 的阶段窗口：M1-① 达成，②③④ 转常设挂账（承接载体逐条点名，禁静默）。

### 1.1 输入说明与 CO 逐条处置

输入来源：`versions/v2.26/REVIEW-r2.md`（2026-10-08，通过；其 §5 遗留问题清单 + §6 v2.27 必含项为本表全量输入）。分级沿用该记录登记（四条均 CO-M）。CO-1…CO-5(v2.26) 见行 8-12（r2 已关闭，本版仅复核不重做）。

| # | 问题摘要 | 来源 | 处置 | 理由 / 触发条件 |
|---|---|---|---|---|
| 1 | CO-6(v2.26)-M 交付 §7.4⑧(:111) 与 §7.6(:131) 称 `--verify` 「R6a 实测 + R6b 复跑双点同值」，但留痕 `build/final-gates-v226-r2.log` 分段 [3] = `--emit` @`3645a20`、[4] = `--verify` @`ec15356`——「双点」只载一点；且 `3645a20` 提交态 ⑧ 为占位形态，按提交态跑 verify 必红 | r2 评审 §5 | **纳入** P0-1 | 一行级措辞勘误：改「`--emit` @R6a（机读同值）+ `--verify` @R6b（单点绿）」；留痕件属 `build/`（gitignore）不可入仓，故以文档措辞承载。实质无影响：P1-2 条文规定的绿态定义时点 = 定稿链末笔，已由 r2 评审独立复现 PASS |
| 2 | CO-7(v2.26)-M 四处机械声称与实测不齐：(i) §8(:147) 「diff 实证 +24 行」vs 实测 numstat `26+/0−`；(ii) §4(:63) 「触碰面」枚举 7 项 vs 实测 14 文件（缺 6 档登记行面）；(iii) §7.1(:85) 「2 红」vs 实测 `fail=1`（一条断言红、双诊断行）；(iv) §5.3(:69) 只提示 (d) 值时点，实测主档 @HEAD 陈旧面 = **(b) 全量 / (b) 自排除 / (d) / token 三面** | r2 评审 §5 | **纳入** P0-2 | 一行级勘误 ×4（与 P0-1 同批，同档同窗口）；机制建议（rN/勘误轮「diff 行数 / fail 计数 / 文件枚举 / 时点差覆盖面」类声称一律按值标注口径）与既有「按值声明」纪律同源，入本版 HC-5/HC-8 条文 + 两模板一行级（P0-3） |
| 3 | CO-8(v2.26)-M rN 文档落 `versions/` 后的机检定位切换：docsync [自指](:551/:553) 与 docref 无参定位(:65/:67) 均 `ls … DELIVERY*.md \| head -1`，`-` < `.` 使 **DELIVERY-r2.md 取代主档**成为校验对象（RP5 两点互证：改主档不红、改 rN 档红）；后果二 = 无参 docref 在 rN 存在时结构性恒红；后果三 = rN 档 §4 区间数字无机检口径 | r2 评审 §5 | **纳入（条文化）+ 机制入池（P0-4）** | 本版裁决 = **不在本版改工具**：① 现网影响面仅在「同版存在 rN 档」时出现，本版计划无 rN 轮；② 三条修法（[自指] 遍历全档 / docref 无参回退或并列 / gatecheck `--from <sha>`）均属门判定语义变化，应与 v3 破坏性窗口同批评（M2）；③ 本版先做**条文显式化**（评审记录模板 §1 + 开发计划模板纪律行各一行：rN 档存在时 [自指]/docref 无参对象 = `head -1` 命中的 rN 档，主档声明不再被机检）+ 入 v3-candidates 池，关闭"盲区未登记"这一半 |
| 4 | CO-9(v2.26)-M r2 的 T-6 给 docsync 新增 26 行断言块并改 architecture（344→347），超出冻结计划 HC-1/HC-7 允许面（原文限 `:198/:199` 两行）；授权来源 = 评审 §6 T-6 + Owner 调用 B2，但交付偏差表 5 项未列「计划面外授权」 | r2 评审 §5 | **纳入** P0-3 | 条文入**权威载体**而非仅本版 PLAN：`docs/开发计划模板.md` HC 指导条增「评审条件清单点名的门新增/门面变更 = 允许面内，须同批 architecture 回填 + N 复算 + docref 登记」；`templates/DELIVERY.md` 偏差表固定列「计划面外授权（来源 = REVIEW-rN T-n）」。**本版即为该条文的首个实例**：本版门改动全部在 HC-7 逐文件授权面内（含新增断言块的 K 值） |
| 5 | M1 窗口 v2.25~v2.27 于本版到期（④ 路盘点） | ROADMAP §当前阶段目标 | **纳入** P0-5 | Owner 2026-10-08 裁决：新建 M3「板上自证与可观测性」承接本版交付项；M1 ②③④ 移出阶段判据、转常设挂账条目（逐条点名承接载体）；M1-① 结案注记 |
| 6 | CO-1(v2.25) 同族防复发面：本版为**改库面 + 跑板侧窗口**的版本，逐版状态登记义务面扩大（G474 现无常设结构化条目，仅 `port/stm32g474/README.md:103/:104` 散文态「维持 v2.10.0 同步态」声称） | v2.26 评审承接链 + 规划者起草期实测 | **纳入** P2-4 + PF-1 | 窗口跑完后该散文声称即为失实文本，必须同批改写；PF-1 新建 G474 常设条目 + v2.27 状态行（Owner 可整条删除，删除后果 = 只改散文行、逐版状态列继续缺位） |
| 7 | 速记区空 / REQ-2、3、4 触发未现 | BACKLOG | **暂缓（维持）** | 触发逐条 = WCH-Link 枚举在位（REQ-2）/ f103 补板或 Renode 深化判据（REQ-3）/ 第二路独立串口资源（REQ-4）；本版认领裁决不覆盖，禁静默 |
| 8 | CO-1(v2.26) 散文占位符入终态 | r1 评审 §5 | **已关闭** | v2.26-r2 T-6 已由 docsync 拦截断言承载（本版实跑 `sh tools/docsync.sh` = **347/0**，两行 ok 点名 `versions/v2.26/DELIVERY-r2.md` 与 `DELIVERY.md`）；本版不重做 |
| 9 | CO-2(v2.26) 锚点式抽检终点锚失配 + 追加即复测 | r1 评审 §5 | **已关闭** | 两模板纪律行已在位（各 grep = 1，v2.26-r2 T-2）；本版 §7⑤/② 遵新条执行（节号先实测未占用、锚点式抽检用修锚后形态） |
| 10 | CO-3(v2.26) AC-2 计数按值声明 | r1 评审 §5 | **已关闭** | r2 已改按值声明三处形态；本版 HC-5 沿用同口径 |
| 11 | CO-4(v2.26) 三落点链断节（v2.25 复评记录节缺位） | r1 评审 §5 | **已关闭** | r2 已补建（12 数据行同源誊录，节链 v2.5…v2.26 无缺位）；本版 P3-4 承同族义务（「v2.27 复评记录」节同批） |
| 12 | CO-5(v2.26) §4 首笔 SHA 悬空/消息不一致 | r1 评审 §5 | **已关闭** | r2 已按 `git log --format='%s'` 逐字重列；本版 §7② 对基线 SHA 一律 `git rev-parse` 实测后才落笔 |
| 13 | CO-4(v2.8)-M / CO-9(v2.8)-M / CO-7(v2.7)-M / ch32x035 与 f103 挂账 / v3 池四条 | 跨版承接链 | **暂缓（维持现状）** | 触发逐条：docsync 内嵌 `--check` 瞬态漂移 = 复现即计；apidump 守卫阈值重校准 = 公开面精简至参照半数以下（本版公开面**纯追加**，触发必不现）；`CO-7(v2.7)` 分支 A = v3 破坏性窗口（M2）；两板挂账 = 硬件资源在位 |
| 14 | 三件套迁 versions/ + 门适配批 | 承接链 | **已关闭** | v2.25 完成并经评审对拍自证；本版零门路径适配（新增门改动仅为 P1 断言块与 P3 用例数字面，非路径） |
| 15 | REQ-6（P2，et_metrics 运行指标注册表）/ REQ-9（P3，et_sched 周期失准统计）/ REQ-8（P3，裁剪/特性自描述命令） | BACKLOG 需求池 | **纳入** 本版 P1-1 / P1-2 / P1-3 | 三条天然配套（REQ-9 的 miss 计数与 REQ-6 的注册表同族形态；REQ-8 与 REQ-6 共用 dump 命令面与 `debug/` 落点）；REQ-6/8/9 状态位由规划者按 A1 规程改「已纳入 v2.27」（BACKLOG 已预改，见 HC-1） |
| 16 | REQ-5（P2，板侧窗口自动化脚本）/ REQ-7（P3，真固件 IAP 走单常设化） | BACKLOG 需求池 | **本版不认领（BACKLOG 态维持已接受）** | ① 容量：本版已含库面新模块 + 结构体追加 + 板侧全要素窗口 + G0 勘误，再叠 tools 层脚本与真固件 IAP 双单（confirm + 回滚）会使单版超出「每条 AC 可被评审方独立复跑」的可核验边界；② REQ-7 的期望行为本身写明「随 REQ-5 自动化为可选步」——两者解耦认领会留下半条链；③ **如实登记**：G474 板在位使 REQ-7 的额外触发（板侧窗口）在本版**部分出现**，本版仍不认领的裁决由本行显式承载，禁静默；触发 = 下一版认领（随 REQ-5 联动，本版窗口已为脚本提供可复现的手工基线流程） |
| 17 | REQ-1（bench 相对比值化）状态位 | BACKLOG + r2 评审 §6 Owner 动作项 ② | **已关闭（功能面）** | M1-① 已由 v2.25 交付并经 r1/r2 两轮评审独立复现；「条件清偿复评通过」前置已由 `REVIEW-r2.md` 满足。REQ-1 转「已完成」= Owner 动作（铁律 9：状态位不由 Agent 改），本版仅在 BACKLOG 补注记一行 |

### 1.2 本版做什么

1. **P0 G0 勘误与条文批**（评审 r2 §6 点名的 v2.27 必含项）：`versions/v2.26/DELIVERY-r2.md` 一行级勘误五处（CO-6 ×1 + CO-7 ×4，修订登记放行）；两模板 + `templates/DELIVERY.md` 条文三处（CO-7 机制建议 + CO-9 门新增授权链条文）；CO-8 条文化 + 入池；M3 登记入 ROADMAP。
2. **P1 库面功能批**（主题 = 板上自证与可观测性）：新模块 `debug/et_metrics`（第 35 模块，定容命名指标注册表 + dump）；`sys/et_sched` 周期失准计数（结构体尾部追加 + 三个新函数 + 默认容差宏）；`AT+FEATURES` 自描述（32 开关 + 版本三件，编译期 `#if` 静态表 + 纯 format 函数 + 库导出 handler）。
3. **P2 板侧窗口批**（G474，Owner 确认在位）：`移植stm32实机记录.md` §16.2 常设规程**全要素最小集**——七目录 + `et_config.h` + `port/port.h` 全量重拷（跨 16 版债）→ 烧前版本串机检 → 烧录 → `AT+VER` / `AT+SELFTEST 22/22` / `AT+MBRD 0 4` 三形态 `disc=0` / **新增 `AT+FEATURES` + `AT+METRICS` 各一读数** → 证据落盘 → 实机记录 §17 新节 + 板侧同步态声称更新。
4. **P3 常设批与发版**：基线滚动三件事、数字回刷同批（含用例数与模块数八处 + 门两处字面量）、登记链收口、`versions/v2.27/DELIVERY.md`（含「v2.27 复评记录」节 + G474 状态行 + §6 零占位符）、⑧ 封闭枚举与 `--emit`/`--verify` 周期、tag + CI + Release。

### 1.3 明确不做什么（Non-goals）

- **不认领 REQ-5 / REQ-7**（§1.1 行 16，触发条件与理由逐条在该行）。
- **不新增 `et_selftest` 套件**：22 套件维持（板上 `SELFTEST: 22/22`、`tools/docsync.sh:327`、`port/stm32f103/renode/smoke.sh:97`、`docs/architecture.md:13/:103/:108` 字面零滚动）。裁决依据 = 先例（第 32 模块 `et_hist`、第 31 模块 `et_medfilt` 均未加板上套件，由 host 单测 + 配方承载）+ 本版板上验证由 `AT+METRICS` / `AT+FEATURES` 两条新走单项直接承载。若执行中判定需要板上套件，走偏差登记并同步上述六处字面 + 体积表 selftest 行（不得静默改 22）。
- **不改 `et_sched_register` / `et_sched_poll_once` 既有签名**：容差经新 setter 与编译期默认宏提供（§7④ 形状互洽裁决，v2.5 `result()` 教训的起草期拦截）。
- **不做 REQ-9 的「miss 时可选回调」**：REQ-9 原文标「可选」；本版 Non-goal（不引入跨上下文回调约束与新注册面），触发 = 需求实证。调度行为本身零改动（REQ-9 非目标原文承接）。
- **不做 v3 池条目**（含 CO-8 的三条门修法）、不动 `protocol/` `core/` `algorithm/` `drivers/` `storage/` 五目录、不做浮点格式化（`ET_MODULE_*` 自描述输出用整数/字符串，`PROCESS` 设计边界内）。
- **不改四门判定语义**：`docref.sh` / `gatecheck.sh` / `sizecheck.sh` 零 diff；`apidump.sh` 仅基线 BASE 三处随 P3-1 滚动（模板 §7⑥ 常设，语义零变更）。
- **历史文档冻结**：v1.1~v2.25 平铺档与 `versions/v2.25/` 零触碰；`versions/v2.26/` 仅 `DELIVERY-r2.md` 勘误（HC-4 放行面）+ `REVIEW-r2.md` 入仓零改动。
- **ROADMAP 仅动 M1 结案注记 + M3 新建**（P0-5），其余候选阶段不动。

## 2. 硬性约束（全量档：数据/行为类 + 流程自指类全在位）

- **HC-1 范围封闭（逐文件允许面，超出即偏差登记）**：
  - 库面：`debug/et_metrics.h`、`debug/et_metrics.c`（新增）、`sys/et_sched.h`、`sys/et_sched.c`、`test/test_metrics.c`（新增）、`test/test_sched.c`（若失准用例并入既有 sched 套件，二选一以执行期实测形态登记）、`test/test_main.c`（extern + `g_suites[]` 各一处）；`et_config.h` 仅版本宏行（`:20/:22`）+ `ET_MODULE_METRICS` + 调参宏（`ET_METRICS_*`、`ET_SCHED_MISS_TOL_MS`）；`examples/posix_demo.c`（可裁量：注册新命令做 host 演示）。**`protocol/` `core/` `algorithm/` `drivers/` `storage/` 五目录 diff = 空。**
  - tools：`debug/…` 构建经 Makefile 变量（见下）；`tools/docsync.sh` = P1-1 新模块断言块（K 行）+ P3-2 用例数字面两处（`:289/:291`）；`tools/apidump.sh` = BASE 三处（`:14/:22/:227`）；`tools/{docref,gatecheck,sizecheck}.sh`、`tools/bench.c`、`tools/*.py` **零 diff**。`Makefile` = `DEBUG_SRC`(`:44`) + `TEST_SRC`(`:49-55`) 两列表项（port/CI 构建用 glob，不需改；`make test` 用显式列表，必改）。
  - 文档面：`README.md`（版本行 `:4` + 交付链接行 + 基线名 `:193/:238` + 特性表 + 用例数 `:59/:185`）、`docs/API_GUIDE.md`（目录 + `### 4.2` 内追加 + 新 `### 8.5` + `## 10` 配置项 + 新 `### 11.15` + 用例数 `:1438/:1451/:1459`）、`docs/API_INVENTORY.md`（重生成）、`docs/API_INVENTORY_v2.26.md`（新增归档）、`docs/architecture.md`（分层框 `:12` + 选型表 `:74/:103` 区 + 验证金字塔 `:110` + 机制门行 `:112`）、`docs/getting-started.md:40`（用例数）、`docs/v3-candidates.md`（「v2.27 复评记录」节 + G474 条目 PF-1 + CO-8 池条目）、`docs/开发计划模板.md` + `docs/评审记录模板.md`（P0-3/P0-4 一行级）、`templates/DELIVERY.md`（P0-3 固定列）、`port/stm32f103/README.md` + `port/stm32g474/README.md`（v2.27 体积行 + g474 同步态声称 `:103/:104`）、`CHANGELOG.md`（`[2.27.0]` 节 + 第 35 模块条目）、`移植stm32实机记录.md`（**§17 新节**，实测节号未占用）、`ROADMAP.md`（P0-5）、`BACKLOG.md`（REQ-6/8/9 已纳入 + REQ-5/7 未认领注记 + REQ-1 状态位「已完成」+ REQ-8/9 注记，规划者已预改，P0-6）、`versions/v2.26/DELIVERY-r2.md`（P0-1/P0-2）、`versions/v2.26/REVIEW-r2.md`（入仓，零改动）、`versions/v2.27/` 三件套。
  - 库外（不在本仓 git，不计入 HC-1 diff 面）：`D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\et\**`（同步目录）、`D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\Src\et_demo.c`（私有 demo）、`D:\code\STM32CubeMX\G474VET6_ET_TEST\build\v2.27-evidence\`（证据目录，写法沿 `v2.10开发交付…:93` 规范形态）。
- **HC-2 git 锚定与触发面**：基线 = tag `v2.26`（实测 `git rev-parse v2.26` = `074759f`，≠ HEAD `ec15356`）；快照 `snap-2.27-start`（**无 `v` 前缀**）打于入仓批末笔，**打后必实测触发面**（`gh run list --limit 3` 无新 run、`gh release list` 无新增，结果入交付——禁"无副作用"先验断言）；分支 `dev/v2.27`；发版 tag `v2.27` 在全部改动定着 `main` 之后。
- **HC-3 证据面**：⑧ 机读值 = `sh tools/gatecheck.sh 2.27 --emit` **逐字粘贴（禁重打）**；`--verify` 绿态定义时点 = 打 tag 前定稿链末笔（P1-2 条文，v2.24 口径），tag 后红 = 预期态按 §7.5 对账；仓库状态类声称 = 封闭枚举四类（(a) 状态值 / (b) diffstat 三数**全量含本文档 + 自排除并列输出** / (c) G0 窗口在最终右端 / (d) 逐文档 docref 判定清单）终态复测，留痕 `build/final-gates-v227.log`。
- **HC-4 修订登记放行**：对已入仓文档的实质变更 = `versions/v2.26/DELIVERY-r2.md`（P0-1/P0-2 五处）一处档面，走「修订登记 @`<sha>`」放行，token 落该档登记行（短语行与 token 行**分行书写**，遵碰撞规避纪律）。该档 docref 形态 = **缺声明行**（沿 v2.26-r2 偏差 3 先例），归入 (d) 既定桶；**注意 CO-8(v2.26) 已登记面**：本版 V=2.27 的 [自指]/docref 无参定位对象 = `versions/v2.27/DELIVERY.md`，不指向 v2.26 档，故本条改动不触发定位切换（若本版后续产生 `DELIVERY-r2.md`，该档即成为校验对象——P0-4 条文化承载）。
- **HC-5 数字回刷同批（门强制面 ⊆ 本清单，逐项按值标注口径）**：`et_config.h` MINOR 27 / `0x021B00`；README 版本行 + 交付链接行（→ `versions/v2.27/DELIVERY.md`）+ 基线名前滚 ×2（→ v2.26 归档）；API_GUIDE 适用版本；CHANGELOG `[2.27.0]` 日期 = 终态提交日；architecture 机制门行 = N 预置复算值（§7 合规声明）；**用例数 `497` 系回刷面 = 实测 8 处**（`docs/getting-started.md:40`、`README.md:59/:185`、`docs/architecture.md:110`、`docs/API_GUIDE.md:1438/:1451/:1459`、另 `README.md:181` 行以终态 grep 实测为准）+ 门字面两处（`tools/docsync.sh:289/:291`）；**五处一致断言面**（docsync `:458-474`）须同时成立；**模块计数 `34`→`35`**（`docs/architecture.md:74`「34 模块按场景查」+ README 特性表行数 + CHANGELOG「第 35 模块」）与**开关计数 `31`→`32`**（实测 `grep -c '^#define ET_MODULE_' et_config.h` = 31）；两 `port/*/README.md` 体积行以 `arm-none-eabi-size` 构建实测回填（`sizecheck.sh:64-77` 逐值比对，缺行即 FAIL）；`docs/API_INVENTORY.md` 工作清单重生成（`### 宏 (51)` 类计数随新增宏滚动，docsync `:234` 内嵌 `apidump --check`）。
- **HC-6 契约零破坏（MINOR 只追加）**：`apidump --check` exit 0；`apidump --diff` 对默认基线 v2.26 归档 = **修改/删除 0 项，新增项 = 本版点名清单**（`et_metrics` 全部公开面 + `et_sched` 三函数 + 新增宏；**判据形态为"新增 = 点名清单"而非 0/0**——本版为库面追加版，写 0/0 即判据与实现冲突，遵模板判据自洽性检查）；既有签名零删改（`et_sched_register/_poll_once/_next_due/_reset/task_stats/task_stats_reset` 六者逐字不变）；`et_task_t` 字段追加须同步 `docs/API_STABILITY.md` §5.1 登记区新行（`:68` 规则：apidump 对结构体字段追加不可见 → 人工登记，v2.2 `et_task_t` 即先例行 `:78`）；模块数 35、开关数 32 与 HC-5 同批。
- **HC-7 门改动边界（本版显式授权面，CO-9 条文的首个实例）**：`tools/docsync.sh` 允许面 = ① P1-1 新模块断言块（**纯新增 K 行**，形态沿 `:334-343` et_hist 先例，既有断言零改动）；② P3-2 用例数字面 `:289/:291` 两行数值。二者**判定语义零变更**（不新增/不移除判定，只扩检查面与刷新数值），architecture 机制门行同批回填 + N 复算 + docref 登记（= CO-9 条文三件套）。其余三门 + `apidump.sh`（除 BASE 三处）+ `Makefile`（除 HC-1 两列表）零 diff。若执行中发现门红源于门的真实判定（非本版改动），走偏差登记处置、**禁静默回退**。
- **HC-8 如实性与时点纪律**：未做即声明；时点性声称带时点限定且不声称本机未跑的读数；本版新增四类声称按值标注口径——`diff` 行数（numstat 增删）/ 非空行数 / `grep -c` 行计数 / 出现次数，四者不混用（CO-7(v2.26) 直接教训）；文件枚举写「全列」或「点名任务面」二选一显式标注；时点差声称标明覆盖面是哪几类（(b)/(c)/(d)/token），不写单类。
- **HC-9 分层与不变式**：`core/algorithm` 零 port.h 不变式保持（本版不动这两目录）；**依赖方向单向向下**（`docs/architecture.md:28`：`core/algorithm ← sys/protocol ← storage/drivers/debug ← 应用`）——`sys/et_sched` **不得** include `debug/et_metrics.h`（反向依赖即违此条）：失准计数与注册表的联动只在**应用侧/板侧 demo** 完成；`debug/et_metrics` 可用 `core/et_smap`（字符串键定容映射，实测 `core/et_smap.h:59-107`）与 `port.h` 临界区（`port/port.h:35` `PORT_CRITICAL_ENTER()`，先例 `sys/et_event.c:23`）；零动态内存、多实例句柄化（注册表存储由调用方提供，形态同 `et_smap_init`）；ISR-safe 范围**逐头文件显式标注**（不变式 `:31-32`；标注形态先例 `sys/et_wdt.h:11` 的 `🔒ISR-safe` / `🏠MAIN` 双标）。
- **HC-10 板侧义务（本版新增形态，来源 = 模板 §2 真机证据条 + Owner 裁决）**：① 板侧读数不得伪造——走单读数一律取自串口实际捕获并落盘 `v2.27-evidence\`，库内文档只引用其摘要；② **烧前版本串机检不过不得烧录**（`移植stm32实机记录.md:808-809`；机检须排除工具链串干扰，` :838` 教训形态 `['2.10.0','2.42.0']`）；③ 走单证据取自**不含下一版代码**的提交（模板 §2 line 54）→ 版本宏回刷（P3-2(a)）必须先于烧录，板上 `AT+VER` 读数 = `v2.27.0`；④ 库外改动全部按 §7③ 标注为私有文件并写全路径，禁止写成库内 `examples/`；⑤ 若板侧构建或走单出现红 = 真实发现，按 `templates/BLOCKED.md` 停机登记，不并入交付自验。

## 3. 任务分解

> 任务号 Pn-m；来源必标（REQ / CO / M / PF / 常设）。提交编号 C-n 由执行者按实际切分登记。**本版有板侧落点**（P2，全路径见 HC-1 末行）。

### P0 G0 勘误与条文批

- **P0-1（CO-6）`DELIVERY-r2.md` verify 双点措辞勘误**：`versions/v2.26/DELIVERY-r2.md` §7.4⑧(:111) 与 §7.6(:131) 的「R6a 实测 + R6b 复跑双点同值」→「`--emit` @R6a（机读同值）+ `--verify` @R6b（单点绿；R6a 提交态 ⑧ 为占位形态，按提交态跑 verify 必红——留痕件在 gitignore 的 `build/` 内，不可入仓）」。判据 = 原「双点同值」声称串于该档 `grep -c` = **0**（判据串不含判据行自身字面，遵证据表述纪律）+ 更正注记行在位 + docref 对该档判定 = 缺声明行（预期态，HC-4）。来源 = `CO-6(v2.26)`。
- **P0-2（CO-7）同档四处按值勘误**：(i) :147「+24 行」→ numstat **26+/0−**（并按值注 non-empty 行数口径，评审实测非空 25 = 注释 10 + 代码 15 + 空行 1）；(ii) :63「触碰面」→ 改标口径为**点名任务面 7 项 / 触碰文件全列 14**（补齐 6 档登记行面）；(iii) :85「2 红」→「1 条断言红（双诊断行）」；(iv) §5.3 :69 → 补「主档 @HEAD 陈旧面 = (b) 全量 / (b) 自排除 / (d) / token **三类**（非仅 (d) 值）」。判据 = 四处逐处 grep 命中 + 值与 r2 评审实测逐项一致（26 / 14 / fail=1 / 三面）。同批走 HC-4 修订登记。来源 = `CO-7(v2.26)`。
- **P0-3（CO-9 + CO-7 机制建议）三处条文**：① `docs/开发计划模板.md` §2 指导条增一行「评审条件清单点名的门新增/门面变更 = 允许面内，须同批 architecture 回填 + N 复算 + docref 登记」；② `templates/DELIVERY.md` 偏差表固定列「计划面外授权（来源 = REVIEW-rN T-n）」；③ 两模板纪律行增「rN/勘误轮中 diff 行数 / fail 计数 / 文件枚举 / 时点差覆盖面类声称一律按值标注口径」（与既有「按值声明」条同源，**纯追加、不改写既有条文**，沿 v2.26-r2 T-2 形态）。判据 = 三处各 `grep -c` = 1 且追加形态经旧行为新行严格前缀程序比对（禁改写）。来源 = `CO-9(v2.26)` / `CO-7(v2.26)`。
- **P0-4（CO-8）rN 机检定位盲区条文化 + 入池**：`docs/评审记录模板.md` §1 增一行「rN 档存在时 [自指]（`docsync.sh:551/:553`）与 docref 无参定位（`docref.sh:65/:67`）对象 = `ls … DELIVERY*.md \| head -1` 命中的 **rN 档**，主档声明不再被机检；无参 docref 恒红属预期态」；`docs/开发计划模板.md` 纪律块同批一行；`docs/v3-candidates.md` 登记三条门修法为 v3 池条目（[自指] 遍历全档 / docref 无参回退或并列 / gatecheck `--from <sha>`），并注明本版裁决理由（HC-1 门槛 = 门判定语义变化应随 M2 同批）。判据 = 两模板各命中 1 + 池条目 1 节 + 本版**零工具改动**（`tools/*` 除 HC-1 授权面外零 diff）。来源 = `CO-8(v2.26)`。
- **P0-5（M3）ROADMAP 修订**：`ROADMAP.md` ① 项目状态行更新（v2.26 已发布 + r2 闭环通过）；② M1 结案（① 达成注记；②③④ **移出阶段判据转常设挂账**，逐条点名承接载体 = `docs/v3-candidates.md` ch32x035 条目 / f103 挂账行 / `CO-8(v2.8)-P`）；③ 新建 **M3「板上自证与可观测性」**（窗口 v2.27~v2.29，判据四条：① 命名指标注册表落地且板上 dump 有读数；② 调度失准计数在板有读数；③ 板侧自描述命令在板可枚举开关与版本三件；④ G474 同步债清偿并确立逐版窗口义务；② 触发项 = REQ-5 板侧自动化，未排期不入判据）。判据 = `grep -c '^\*\*M3'` = 1 + M1 行含「转常设挂账」字样。来源 = `M-3` + Owner 2026-10-08 裁决。
- **P0-6 BACKLOG 同步（规划者已预改，随入仓批）**：REQ-6/8/9 → 「已纳入 v2.27」；REQ-5/7 → 维持已接受并补「v2.27 未认领理由 = 计划 §1.1 行 16」；REQ-8 补**数字失实注记**（原文「34 个 `ET_MODULE_*` 开关」实测 = **31**，34 是模块数，本版后 35）；REQ-9 补**形状拦截注记**（「容差注册时可配」若落 `et_sched_register` 即改签名）；**REQ-1 → 「已完成」**（M1-① 交付 + r1/r2 两轮评审独立复现，前置条件已满足；状态位原为 Owner 动作，**2026-10-08 Owner 明示授权规划者代改**，注记内署名归因与日期在案）。来源 = A1 规程 + `CO-7(v2.26)` 同族（数字失实）+ Owner 授权。
- **G0 窗口判据**（右端时点化 = G0 末笔提交 SHA 实测回填）：窗口谓词**逐文件枚举既有门**（`tools/docref.sh` / `gatecheck.sh` / `apidump.sh` / `sizecheck.sh`）+ 库面六目录 + `Makefile` = diff **空**；`tools/docsync.sh` 显式排除并归因（本版 P1-1/P3-2 计划内，非 G0 面）；G0 触碰面 = `versions/v2.26/DELIVERY-r2.md` + 两模板 + `templates/DELIVERY.md` + `docs/v3-candidates.md` + `ROADMAP.md` + `BACKLOG.md`，实测非空并写明归因；docsync G0 终态 = 复算值（预期仍 347/0——G0 零新增断言，P0-3/P0-4 为文档面）；docref 对 `DELIVERY-r2.md` = 缺声明行（预期态）。

### P1 库面功能批（主题：板上自证与可观测性）

- **P1-1（REQ-6）新模块 `debug/et_metrics.{h,c}`——定容命名指标注册表（第 35 模块）**：
  - 形态：`et_metrics_t` 句柄 + 调用方提供的 slot 数组与键存储池（零分配、多实例，init 形态同 `et_smap_init`）；两类指标（u32 计数器 / u32 仪表）+ 可选 `et_hist` 实例挂接（观测值入桶）；键名上限复用 `ET_SMAP_KEY_MAX` 量级或独立 `ET_METRICS_KEY_MAX`（可裁量，须在 §10 配置项登记）。
  - API 形状（纯追加，实现细节可裁量但不得破坏下列断言对象）：`init` / `register_counter` / `register_gauge` / `link_hist` / `inc` / `add_n` / `set` / `observe` / `get`（未登记如实报 0 或 found=false）/ `count` / `iter`（按 idx 取回 key/value/kind）/ `reset` / **`format(m, buf, cap)` → 返回写入长度**（dump 唯一渲染源，host 单测逐字断言对象）；库导出 handler `et_metrics_dump_cmd(char *args, void *user)` 与 `et_features_cmd(...)`，签名 = `et_atcmd_fn`（实测 `protocol/et_atcmd.h:27` `typedef void (*et_atcmd_fn)(char *args, void *user)`），注册形态沿既有先例 `{"HELP", et_shell_help_cmd}`（实测 `examples/stm32g474_demo.c:281`，表块 `:276-282`）——**库不持有命令表**的惯例保持不变。
  - 并发标注：`inc/add_n/observe/get` = `🔒ISR-safe`（单字自增，前提「同一指标由单一上下文自增」，与 `core/et_ringbuf.h:11` / `sys/et_wdt.h:11` 同族显式声明）；`init/register/format/dump` = `🏠MAIN`。若采用临界区保护聚合面，用 `PORT_CRITICAL_ENTER()`（HC-9 允许 `debug/` 触 port）。
  - 配套义务（新模块惯例五面）：`et_config.h` 开关 `ET_MODULE_METRICS`（默认 1）；`Makefile:44 DEBUG_SRC` 追加 `debug/et_metrics.c`；`Makefile:49-55 TEST_SRC` 追加 `test/test_metrics.c` + `test/test_main.c` extern 与 `g_suites[]` 各一处（精确先例行实测 = `:20` `extern … test_sched_cases` / `:64` `{ "sys/sched", test_sched_cases },`，新条目形态 `{ "debug/metrics", test_metrics_cases }`）；`README.md` 特性表行 + `docs/API_GUIDE.md` 目录 + **新 `### 8.5 et_metrics 运行指标注册表`（实测 §8 现至 8.4，节号未占用）** + `## 10` 配置项 + **新 `### 11.15` 配方（实测现至 11.14）**；`docs/architecture.md:12` debug 框 + `:74` 选型表行；`tools/docsync.sh` 新模块断言块（形态沿 `:334-343`，K 行实测）。
  - 非目标承接：不做掉电保持、不做浮点/均值聚合（交 `et_stats`）、不做动态订阅/广播；**不强制改既有模块**（既有 stats 选择性登记在应用侧完成）。
  - 来源 = `REQ-6` + `M3-①`；文档面义务来源另标「常设（README 清单①：无断言的同步声明视为未同步）」。
- **P1-2（REQ-9）`sys/et_sched` 周期失准计数**：
  - 结构体：`et_task_t` 尾部追加 `miss_cnt` + `tol_ms`（`sys/et_sched.h:35-45`，追加标记注释行沿用 `:42` 形态）；**`docs/API_STABILITY.md` §5.1 登记区新行**（`sizeof` 增大半径与 v2.2 先例行 `:78` 同格式）。
  - 判定：`et_sched_poll_once`（`sys/et_sched.c:73-111`）在到期判定 `:85` 与重锚 `:95` `hit->last_run = now;` **之间**用覆写前 `elapsed` 判定 `elapsed > period_ms + tol_ms` → `miss_cnt++`；调度行为（补跑一次 + 重锚）零改动。
  - API：`et_sched_task_miss(const et_task_t *t, uint32_t *miss_cnt)`（输出指针可 NULL，同族形态 = `et_sched_task_stats` `:74`）、`et_sched_task_miss_reset(et_task_t *t)`、`et_sched_task_set_tolerance(et_task_t *t, uint32_t tol_ms)`；默认容差宏 `ET_SCHED_MISS_TOL_MS`（`et_config.h`，§10 登记）；`et_sched_register` **签名不变**（§7④ 拦截）。
  - 已知边界（文档明示，REQ-9 非目标原文）：主循环停转期间漏检属已知边界；API_GUIDE `### 4.2` 内追加口径说明 + miss 与 last/max 的语义差别。
  - 单测：`test/test_sched.c` 增 miss 计数用例（虚拟时基推进可复现，host 侧确定性断言）。来源 = `REQ-9` + `M3-②`。
- **P1-3（REQ-8）`AT+FEATURES` 裁剪/特性自描述**：
  - 编译期 `#if` 生成的**静态描述表**（32 个 `ET_MODULE_*` + `ET_VERSION_MAJOR/MINOR/PATCH` + `ET_VERSION_STRING`），落在 `debug/et_metrics.c`（同模块承载，避免第二个新模块）；未启用模块**如实报 0**。
  - 面：纯函数 `et_features_format(char *buf, uint32_t cap)`（返回写入长度，host 逐字断言对象）+ handler `et_features_cmd`（经 `ET_LOGI` 或 `et_shell_puts` 输出，可裁量）。
  - **数字校正**：需求原文「34 个 `ET_MODULE_*` 开关」失实——实测 `grep -c '^#define ET_MODULE_' et_config.h` = **31**，本版 +1 = **32**；34/35 = 模块数（`CHANGELOG.md:364`「第 34 模块」先例）。已按 P0-6 注记回写 BACKLOG。
  - 注册面：`examples/stm32g474_demo.c` / `examples/posix_demo.c` 命令表注册（可裁量，host 演示非强制）+ 板侧私有 `et_demo.c` 注册（P2-1，两条命令）。来源 = `REQ-8` + `M3-③`。

### P2 板侧窗口批（G474，规程全要素最小集）

- **P2-1 同步与私有 demo 改动（库外）**：`Core\et\` 全量重拷七目录 + `et_config.h` + `port/port.h`（`et_port/`、`et_demo.c` 私有不动，实测规程 `移植stm32实机记录.md:278` + 命令块 `:436-446`）；**跨 16 版债**：`Core/et/et_config.h:20` 实测 MINOR = 10，须刷新至本版值，`diff -rq` 结果**实测登记、勿凭记录断言**（`:806` 明文）；私有 demo `Core\Src\et_demo.c` 增：`et_metrics_t` 静态实例 + 若干指标登记（含从既有模块 stats 桥接的示例，展示 HC-9 应用侧联动路径）+ 命令表注册 `METRICS` / `FEATURES` 两行——**板上命令表条目数以开工时该文件实测为准**（该文件在库外、不在本仓 git，禁凭本仓记录断言其条数；历史走单已见 `AT+VER`/`BOOTINFO`/`RXSTAT`/`SELFTEST`/`MBRD`/`MBSLAVE`/`MBRAW` 等，实机记录 `:70/:751/:852`）。来源 = `M3-④` + 常设（模板 §2 真机证据 / §7③ 板侧全路径）。
- **P2-2 构建与烧前机检**：私有工程构建（`build.log` 落盘）→ ELF 版本串机检 = 预期 `v2.27.0`（排除工具链串干扰，`:838` 形态）；**不过不得烧录**（HC-10②）。若构建红（16 版 API 漂移暴露）= 真实发现，走 `templates/BLOCKED.md` 停机登记（HC-10⑤），禁改库面绕过或伪造读数。来源 = 常设（§16.2 规程 step 烧前机检）。
- **P2-3 烧录与走单**：`STM32_Programmer_CLI -c port=SWD -w … -v -rst`（CLI 实测在位 = `D:\software\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe`，规程行 `移植stm32实机记录.md:814-817`）→ 走单五项：`AT+VER`、`AT+SELFTEST`（**22/22**，本版不加板上套件的裁决见 §1.3）、`AT+MBRD 0 4` **三形态 `disc=0`**（形态口径按值声明：§16.4 实跑表行 = CRLF / 仅 CR / 仅 LF **三形态**（`:844-846`，结论行 `:853` 记「严于 AC-7 双形态要求」），而规程条文行 `:816` 措辞为"两形态"——本版按**三形态**执行并在 §17 逐行登记，**不改 §16 历史文本**）、**新增 `AT+FEATURES`**（32 开关 + 版本三件逐行读数）、**新增 `AT+METRICS`**（含 `miss_cnt` 读数，M3-② 的板上第一手数据）；端口**重新枚举**（勿凭记录直连——最后一次全量枚举 = v2.22 时点证据件，落盘路径载于 `docs/v3-candidates.md:387`；该次读数 = COM12 CH343[G474] 在位且为单路、COM16（WCH-Link/ch32x035）不在位，跨 4 版未复核）。证据落盘 `v2.27-evidence\`（`diff_rq.txt` / `sync_sha256.txt` / `build.log` / 走单捕获 + 时间戳 + 逐项摘要，与手工留痕同构可被评审核验；驱动脚本 `walk.py` 不入库、原始全文 = `walk_output.txt`，沿 `:840` 形态）。来源 = `M3-①②③④` + 常设（§16.2 走单与落盘）。
- **P2-4 实机记录 §17 与板侧声称更新**：`移植stm32实机记录.md` **新 §17**（实测节号未占用：现至 §16，子节 16.1-16.4 已占）——含 §16.2 规程逐步读数、跨 16 版同步债清偿登记、两条新命令走单原文；同批改写 `port/stm32g474/README.md:103/:104` 散文态「板侧维持 v2.10.0 同步态」（本版后为 v2.27.0 同步态实测，**不改写即成失实文本**）；两 port README `v2.27` 体积行构建实测回填（f103 两行 default + selftest，g474 一行 default）。**PF-1（可整条删除）**：`docs/v3-candidates.md` 新建 **G474 常设结构化条目**（逐版状态列 + 触发 = 每版改库面即窗口义务），并落 v2.27 行——动机 = 现仅散文态声称、无常设条目，v2.27 窗口后逐版状态无承载面（CO-1(v2.25) 同族风险）。来源 = 常设（模板 §7⑤ 证据落点章节号）+ `CO-1(v2.25)` 同族防复发 + PF-1。

### P3 常设批与收口

- **P3-1 基线滚动三件事**（常设，模板 §7⑥）：① `docs/API_INVENTORY_v2.26.md` 归档——`git worktree` 于 **tag v2.26**（实测 `074759f`）真生成（禁手工编辑）+ blob 层 sha256 + 与 `API_INVENTORY_v2.25.md` 归档 diff **逐段列出**（v2.25→v2.26 公开面零变化，预期 diff = 仅 `ET_VERSION_MINOR` 宏行 `25→26`，1 删 1 增；出现超出即公开面未登记变化，计偏差）+ `item_lines` 口径值实测登记；② `tools/apidump.sh` BASE 切 v2.26（`:14/:22/:227` 同批，履历续写；docsync 基线断言 `:385-401/:501-512` 动态承载，零字面量额外改动）；③ 发版点对本版工作清单复读 = 对 **v2.26 基线**的 `--diff`「**修改/删除 0 项，新增 = 本版点名 API 清单**」（HC-6 形态；本版为追加版，写 0/0 即判据与实现冲突）。
- **P3-2 数字回刷同批**（HC-5 全清单，**分两段**）：**(a) 版本宏段（必须先于 P2-2 烧录）** = `et_config.h` MINOR 27 / `0x021B00` + README 版本行 + 交付链接行 + 基线名前滚 ×2 + API_GUIDE 适用版本 + CHANGELOG `[2.27.0]`（日期 = 终态提交日）；**(b) 其余数值段（随定稿批）** = 用例数八处 + 门两处（497→实测）、模块数 34→35、开关数 31→32、architecture 机制门行 N（预置复算，见 §7 合规声明）、验证金字塔 `:110/:112`、两 port README 体积行实测、`API_INVENTORY.md` 重生成。`(a)/(b)` 切分登记为**分段形态**，前置段清单封闭（模板纪律 v2.21 条）。
- **P3-3 登记链收口**：以 docref 对各 OK 档**实测点名面**为准追加登记 token（本版触碰 `docsync.sh` + 库面 + 两模板 + `DELIVERY-r2.md` 等，点名面禁预抄）；(d) 桶分类值以发版点机读输出为准。来源 = 常设（docref 门断言 / HC-4）。
- **P3-4 `versions/v2.27/DELIVERY.md`**：沿权威模板 §1-§8 结构 + 行首声明「本版 docsync：N/N」（N 终态实测，禁预写）+ 覆盖率行 + §4 记录范围（`snap-2.27-start..` + 终态右端时点化）+ §7.4⑧ `--emit` 逐字节片段 + §7.5 复现预期 + AC 自验表；**同批登记面三件显式列任务**（CO-1/CO-2(v2.25) 同族防复发）：① `docs/v3-candidates.md`「v2.27 复评记录」节（先 `grep -n '^## '` 确认节号未占用；行数 = 本计划 §1.1 同源誊录，两源差集显式对齐）；② G474 逐版状态行（PF-1 采纳时）+ ch32x035/f103 挂账 v2.27 行（本版有板侧窗口但**仅 G474**，ch32x035 触发仍为 WCH-Link，须如实登记「本版窗口 = G474，ch32x035 挂账续期」）；③ 本交付 §6 零裸占位符（docsync `:519-543` 拦截断言为真判定，散文裸占位符即红）。
- **P3-5 定稿链与发版**：DELIVERY 定稿（修订登记合并）→ `gatecheck 2.27 --emit` 逐字粘贴 → ⑧ 封闭枚举四类复测留痕 `build/final-gates-v227.log` → `gatecheck 2.27 --verify versions/v2.27/DELIVERY.md` **PASS @ 定稿链末笔** → (d) 全集重测 → 打 tag `v2.27` → CI 七 job 绿 + Release 四附件 → tag 后 verify 红 = 预期态对账 → 删除 `dev/v2.27`、tag 对齐。若本版产生 rN 轮：先读 P0-4 条文与 CO-8（`head -1` 定位切换）。

## 4. 验收标准（全量档；证据落点 = `versions/v2.27/DELIVERY.md` AC 自验表 + `移植stm32实机记录.md §17` + `build/` 留痕）

- **AC-1（P0-1/P0-2，CO-6/CO-7）**：`versions/v2.26/DELIVERY-r2.md` 中「双点同值」原式 `grep -c` = **0**（判据行不含自身字面）；四处勘误逐值命中 = numstat `26+/0−` / 触碰文件全列 **14** / `fail=1`（一条断言双诊断行）/ 时点差面标 **(b)/(b自排除)/(d)/token 三面**；修订登记 token 在该档登记行（短语行分行）；docref 对该档 = 缺声明行（预期态，HC-4 注记在案）。
- **AC-2（P0-3/P0-4/P0-5/P0-6）**：三处条文各 `grep -c` = 1（开发计划模板 HC 指导条 / templates/DELIVERY 固定列 / 两模板 rN 定位纪律行各 1）；追加形态经「旧行为新行严格前缀」程序比对成立（禁改写既有条文）；`ROADMAP.md` `^\*\*M3` = 1 且 M1 行含「转常设挂账」；BACKLOG REQ-6/8/9 三行含「已纳入 v2.27」、REQ-8 含 31/34 数字注记；`tools/` 除 HC-1 授权面零 diff（`git diff --numstat` 逐文件点名）。
- **AC-3（P1-1 新模块，M3-①）**：`debug/et_metrics.h/.c` 存在且 `#if ET_MODULE_METRICS` 门内；`apidump --diff` 对 v2.26 基线 = **新增项含 `et_metrics_*` 全部公开函数，修改/删除 0 项**；`et_features_format` / `et_metrics_format` host 单测逐字断言在 `test_metrics_cases` 中且 `make test` 全绿；五面文档义务逐项机读命中（README 特性行 / API_GUIDE `### 8.5` + 目录 + `## 10` + `### 11.15` / architecture 分层框 + 选型表 / `Makefile:44` + `TEST_SRC` / `test_main.c` extern + `g_suites`）；docsync 新模块断言块 K 行实测在位且全绿。
- **AC-4（P1-2 失准计数，M3-②）**：`et_task_t` 尾部两字段追加（`sys/et_sched.h`）+ `docs/API_STABILITY.md` §5.1 新登记行 = 1；`et_sched_register/_poll_once/_next_due/_reset/task_stats/task_stats_reset` 六签名逐字不变（`git diff` 对照）；新增三函数存在且 `make test` 中 miss 计数用例以虚拟时基确定性通过（含「`elapsed ≤ period+tol` 不计、`>` 计」、「重锚后不误计」两类）；API_GUIDE `### 4.2` 含主循环停转漏检的已知边界声明。
- **AC-5（P1-3 自描述，M3-③）**：`et_features_format` 输出含**版本三件** + **32 个开关逐行 0/1**（计数按值标注 = `grep -c` 行计数）；未启用模块报 0 由单测以显式关闭某开关的构建变体或表值断言承载（二选一，形态登记）；库不持有命令表（`protocol/et_atcmd.*` 零 diff）。
- **AC-6（P2 板侧窗口，M3-④）**：`移植stm32实机记录.md §17` 存在且含五项走单读数原文（`AT+VER` = `v2.27.0` / `SELFTEST: 22/22` / `disc=0` 三形态 / `AT+FEATURES` / `AT+METRICS` 含 `miss_cnt`）；`diff -rq` 同步实测结果登记（跨 16 版债清偿，非凭记录断言）；证据目录 `v2.27-evidence\` 四件在位（库外路径写全）；`port/stm32g474/README.md` 「v2.10.0 同步态」原式 `grep -c` = **0** 且新声称与 §17 同值；烧前机检留痕在案（含工具链串排除说明）。
- **AC-7（P3-1 基线滚动）**：`docs/API_INVENTORY_v2.26.md` 存在 + blob 层 sha256 登记 + 与 v2.25 归档 diff 逐行列出（公开面零变化预期 = 仅 `ET_VERSION_MINOR` 一行，超出即登记为公开面未声明变化）；`apidump.sh` BASE = v2.26（`:14/:22/:227` 三处实测）；发版点 `--diff` 复读 = 修改/删除 0（新增项与 AC-3 点名清单一致）。
- **AC-8（P3-2 数字回刷）**：HC-5 清单逐项命中——MINOR 27 / `0x021B00`（`gcc -E -dM et_config.h | grep ET_VERSION` 复现）；README 版本行 + 交付链接行 + 基线名前滚 ×2；用例数 8 处 + 门 2 处**同值**（五处一致断言 `docsync.sh:458-474` 绿）；模块数 35 / 开关数 32（`grep -c '^#define ET_MODULE_' et_config.h` = 32 实测）；CHANGELOG `[2.27.0]` 日期 = 终态提交日；architecture 机制门行 = N（预置复算值与终态实测一致，不一致 = 公式缺项如实登记）。
- **AC-9（全量回归与发布面）**：四形态读数实测（当前基线 497/497/504/498 @v2.26，本版随新增用例滚动，**以实测回填、四者逐值标注口径**）+ `make ex` 五配方 PASS；`sh tools/docsync.sh` = pass N/N fail=0（N 终态实测，行首声明对账）；`sh tools/apidump.sh --check` exit 0；`sh tools/sizecheck.sh` = pass=3 fail=0（体积行实测）；CI 七 job success（tag run）+ Release Latest 四附件 + `gh run list`/`gh release list` 快照时点无 `snap-*` 误触发实测（HC-2）。
- **AC-10（⑧ 与 (d) 全集）**：§7.4⑧ 五锚行 = `gatecheck 2.27 --emit` 逐字（`diff` 空，5/5 行）；`--verify` **PASS @ 定稿链末笔**；tag 后 verify 红 = 仅预期差且与 §7.5 逐字吻合；token 机读与 §7.3 一致；(d) 全集 = 31 + 1 = **32 份**（算式：平铺 + `versions/v2.25` + `versions/v2.26` 两档 + 本档，以发版点机读为准，禁预抄分类值）；本档 = OK。
- **AC-11（G0 窗口）**：窗口谓词（§3 P0 逐文件枚举面）于 G0 终态提交实测 = 空，`docsync.sh` 排除归因 + 触碰面归因在案；`et_config.h` 版本宏前 + 五库目录（`protocol/core/algorithm/drivers/storage`）+ 四门 + `Makefile` 零 diff。
- **AC-12（不变式与依赖方向，HC-9）**：`sys/et_sched.c/.h` 不含 `et_metrics.h`（`grep -c` = 0，含头文件与实现两处）；`core/` `algorithm/` 不含 `port.h`（既有不变式复测）；`debug/et_metrics.*` 的 ISR-safe 标注逐 API 显式存在（形态命中 `🔒ISR-safe`/`🏠MAIN` 双标 ≥1）；零 `malloc/calloc/realloc/free` 字面于新增两文件（`grep -c` = 0）。

## 5. 可裁量项与不可偏离

- **可裁量**：① `et_metrics` 内部存储实现（自建定容表 or 复用 `core/et_smap`；临界区取舍；键名上限宏命名）——但 init 存储调用方持有、零分配、`format` 为唯一渲染源三点不得弱化；② handler 输出路径（逐条 `ET_LOGI` vs 单缓冲 `format` 后输出）；③ `test_sched` 用例并入既有套件 or 新建套件；④ `examples/posix_demo.c` 是否注册新命令；⑤ P2 内部提交切分与走单顺序（须保持 HC-10③ 版本串先决条件）；⑥ PF-1 G474 常设条目的字段编排（Owner 可整条删除该 PF，删除后果见 P2-4）；⑦ DELIVERY 章节内部编排（沿权威模板结构）。
- **不可偏离**：§1.1 全部处置结论；§2 全部 HC（尤其 HC-6 六签名不变、HC-9 依赖方向、HC-10 板侧读数与机检先决）；§1.3 全部 Non-goals（尤其**不新增板上 selftest 套件**、**不改 `et_sched_register` 签名**、**不做 miss 回调**、**不认领 REQ-5/7**）；§6 基线规则（含入仓批预申报）；历史文档冻结面（除 HC-4 放行档）；门判定语义（除 HC-7 授权两类改动）。

## 6. 基线

| 字段 | 内容 |
|---|---|
| 首选基线 | `tag v2.26`（实测 `git rev-parse v2.26` = `074759f`；起草时点 HEAD = `ec15356`，r2 批七笔在其后 → 与 §4 记录范围口径需并列声明，见 HC-3 (b) 类） |
| **入仓批（预申报）** | 开工前未入库件 = `versions/v2.26/REVIEW-r2.md`（实测 `git status --porcelain` 唯一未追踪件）+ `BACKLOG.md`/`ROADMAP.md` 规划者预改（P0-5/P0-6）+ 本计划——作为首笔入仓批提交（沿 v2.26 预申报形态）；`snap-2.27-start` 打于入仓批末笔 |
| 快照（动手前必打） | `git tag snap-2.27-start <入仓批末笔>`（**无 `v` 前缀**，理由 = `release.yml` 触发面 `tags: [v*]`，见 §7①）；打后触发面实测（HC-2） |
| 分支规则 | 改动在 `dev/v2.27`；发版 tag `v2.27` 在全部定着 `main` 之后；`dev/*` 残留本版收口时确认清空 |
| **顺序规则（本版含板侧）** | ① 入仓批 → ② P0（G0 窗口，零库面）→ ③ P1 库面功能批 → ④ **P3-2(a) 版本宏回刷**（HC-10③ 先决：板上版本串须 = `v2.27.0`）→ ⑤ P2 板侧窗口（同步/构建/机检/烧录/走单/§17）→ ⑥ P3-1/P3-2(b)/P3-3/P3-4 常设与交付 → ⑦ P3-5 定稿链与发版。板不可达时合法路径 = ⑤ 停机 BLOCKED，⑥⑦ 可先行至「板侧读数缺位」如实挂账形态，但**本版不得据此把板侧项改挂账**（Owner 已确认板在位；挂账的合法性依据 = 板不在 bench，`移植stm32实机记录.md:788`） |
| 偏离申报 | 实际起点 = 入仓批末笔（≠ `074759f`）属**预申报形态**，交付首行仍如实声明；跨 16 版同步债若暴露构建/走单新问题，按偏差逐条登记不并入自验 |

## 7. 起草期核验清单（六项全过）

| # | 检查项 | 本计划执行情况 |
|---|---|---|
| ① | 内部 tag 命名 | `snap-2.27-start` 无 v 前缀（`release.yml` `tags: ['v*']` 冲突面沿 v2.25 实读先例，本版未改该文件）；触发面实测义务写入 HC-2（打后 `gh run list --limit 3` / `gh release list`，禁"无副作用"先验断言）；软重置检查随 B 执行（模板 §7① 重做提交前核对回退点先于待裁除提交） |
| ② | "无副作用/不会触发"类断言逐条实测 | 起草期实测（2026-10-08，A1 对话）：`sh tools/docsync.sh` = **pass=347 fail=0** @HEAD（本对话实跑**两点**：规划产出件写出前一次、`versions/v2.27/PLAN.md` + `BACKLOG.md`/`ROADMAP.md` 预改写出后复跑一次 = **同值 347/0**，规划者不扰动门；两行占位符拦截点名 `DELIVERY-r2.md`/`DELIVERY.md`，自指行实测 347）；**同点 `sh tools/docref.sh`（无参）= FAIL，点名 `versions/v2.26/DELIVERY-r2.md 缺'记录范围 + diffstat'声明行`——即 CO-8(v2.26) 所述「rN 档存在时无参定位结构性恒红」的现网实例复现**（规划者产出件写前/写后各一次同值；本版不以无参 docref 为绿态判据，登记链一律按 HC-4 点名式 `docref <档路径>` 复跑）；`git rev-parse v2.26` = `074759f`、`git tag --points-at HEAD` = 空、HEAD = `ec15356`；`git status --porcelain` = 唯一未追踪件 `versions/v2.26/REVIEW-r2.md`；`grep -c '^#define ET_MODULE_' et_config.h` = **31**；`grep -rn 497` 文档八处定位在案；`examples/ex_*.c` = 5；板侧 `Core/et/et_config.h:20` MINOR = **10**（库内 26）；pyserial 3.5 + `serial.tools.list_ports` import 成立（REQ-5 未认领，此读数仅作登记）；CO-7 四处原式行号 :63/:85/:111/:131/:147 + :69 实测在位。转交付/终态实测：docsync N、用例四读数、体积、`apidump --diff`、板侧五项——均非起草期可断言 |
| ③ | 板侧落点标注 | 板侧改动一律标为**板侧工程私有文件（库外，不在本仓 git）**并写全路径：`D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\Src\et_demo.c`、同步目录 `D:\code\STM32CubeMX\G474VET6_ET_TEST\Core\et\`、证据 `D:\code\STM32CubeMX\G474VET6_ET_TEST\build\v2.27-evidence\`（写法实测核正：`移植stm32实机记录.md:831` 该行反斜杠已丢失，规范形态取 `v2.10开发交付__挂账清偿与板侧闭环.md:93/:132`）；**禁止**写成库内 `examples/`（同步清单 = 七目录 + `et_config.h` + `port/port.h`，实测 `移植stm32实机记录.md:278` + 命令块 `:436-446`，`examples/` 不在清单内——板上烧的是私有 `Core\Src\et_demo.c`，其命令表条目数以开工实测为准，本仓仅见历史走单项 `AT+VER/BOOTINFO/RXSTAT/SELFTEST/MBRD/MBSLAVE/MBRAW`，`:70/:751/:852`） |
| ④ | AC ↔ 冻结 API 形状互洽 | 逐条核过：① REQ-9「容差注册时可配」若落在 `et_sched_register` 即改签名 → 与 HC-6 六签名不变冲突 → **已拦截**，改 `et_sched_task_set_tolerance` + `ET_SCHED_MISS_TOL_MS`（AC-4 断言对象 = 新函数存在 + 签名零变）；② 命令 handler 形状 = 实测 `et_atcmd_fn = void (*)(char *args, void *user)`，`et_shell_help_cmd` 先例在位 → AC-3/AC-5 的 handler 注册形态与之一致；③ `format` 返回写入长度 vs AC-5「逐行 32 开关」——断言对象 = 缓冲内容，与形状可同时成立；④ `et_task_t` 字段追加 apidump 不可见 → 以 AC-4 的 §5.1 人工登记行承载；⑤ `diff` 类判据一律写「新增 = 点名清单」而非 0/0（本版为追加版） |
| ⑤ | 证据落点章节号 + 占位节清理 | 板侧证据落点 = `移植stm32实机记录.md **§17**`（实测 `grep -n '^## '`：现至 §16，子节 16.1-16.4 已占，§17 未占用）；API_GUIDE 新增落点 = `### 8.5`（现至 8.4）与 `### 11.15`（现至 11.14），均实测未占用；「v2.27 复评记录」节 P3-4 落笔前再 `grep -n '^## '` 复核（遵 v2.26-r2 G0-T2 新条：终点锚形态 + 追加即复测）；本版不新增、不取代任何旧占位节（§12 已由 §13 承接的历史形态不动） |
| ⑥ | 基线滚动三件事 | P3-1 显式列出三件：归档 v2.26（worktree 于 tag v2.26 真生成 + blob sha256 + 与 v2.25 归档 diff 逐段）/ BASE 切 v2.26（三处，docsync 动态断言自证）/ 发版点对本版清单复读（HC-6 追加形态，非 0/0） |

---

**起草期合规声明**：六项核验已于起草对话（2026-10-08）执行，实测命令与结果见 §7②（docsync 347/0 @`ec15356` 本对话实跑；tag/HEAD/工作树态/开关数/497 面/§17 与 §8.5+§11.15 节号未占用/板侧 MINOR 落差 = 逐条 grep 或 rev-parse 命中后落笔）。

**预置数值按值声明口径与逐项复算**：
- docsync N 预置 = 上版实测 **347** + 1（本版 `DELIVERY.md` 覆盖率行入循环）+ 1（CHANGELOG `[2.27.0]` 链）+ **K**（P1-1 新模块断言块行数，K 以 diff 实测）= **349 + K**；K 预期 9~12（`docsync.sh:334-343` et_hist 先例形态），**N 终态实测回填，禁预写**（README 清单⑧）。
- (d) 全集 = 31（v2.26-r2 终态实测）+ 1（本版 DELIVERY）+ rN 数（本版预期 0）= **32 份**，分类值以发版点 `--emit` 机读为准。
- 用例数基线 = 497（v2.26 四读数 497/497/504/498，本版随新增用例滚动，四者各自实测）；回刷面 = 文档 8 处 + 门 2 处（`docsync.sh:289/:291`），按值标注口径 = `grep -c` 行计数。
- 模块数 34→35、开关数 31→32（`grep -c '^#define ET_MODULE_'` 行计数口径实测）；板上套件 **22 维持**（§1.3 裁决）。
- 体积行 = `arm-none-eabi-size` 构建实测回填（本版新增 `debug/et_metrics.c` 入 glob，f103/g474 体积必变，禁沿用 v2.26 值）。
- 版本宏 = MINOR 27 / hex `0x021B00`（`et_config.h:20/:22` 现值 26 / `0x021A00` 实测）。

**纪律自查**：判据区间右端时点化（G0 终态 SHA / P3-2 分段前置清单封闭 / 定稿链末笔）；G0 窗口谓词逐文件枚举既有门、禁裸目录（v2.24 条）；含「修订登记」短语的散文行不携带 token（分行形态）；引用失实原文一律入 code span；机读值 `--emit` 逐字粘贴、`--verify` 绿态 = 定稿链末笔；计数类逐值标注口径（行计数 / numstat 增删 / fail 计数 vs 诊断行数 / 时点差覆盖面类别数）——本条即 CO-7(v2.26) 四条教训的本版承载；板侧读数禁伪造且烧前机检为硬先决（HC-10）。

## 变更记录（冻结后只允许追加，需 Owner 批准）

| 日期 | 变更 | 原因 | 批准人 |
|---|---|---|---|
| 2026-10-08 | 初稿（规划者 A1 对话产出，状态 = 待批准）：四路盘点 → 出口 A 功能版全量档；§1.0 ②/§1.1 行 15-17/§1.3/HC-1 同步 BACKLOG 预改（REQ-6/8/9 已纳入、REQ-5/7 不认领）；M3 经 P0-5 登记入 ROADMAP | 立项判定（Owner 裁决：主题批认领 + G474 板在位 + 新建 M3） | Owner（2026-10-08 批准） |
| 2026-10-08 | 状态行「待批准」→「**已批准-冻结**」；P0-6 与 HC-1 的 BACKLOG 面补 REQ-1「已完成」（Owner 授权规划者代改，注记内署名归因）与 REQ-9 形状拦截注记；PF-1（G474 常设条目）经「无意见」= **保留** | Owner standing 规则「当我让你规划下一个版本时，默认批准」；三项动作同日批复 | Owner |
