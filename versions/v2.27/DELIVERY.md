# v2.27 DELIVERY：运行指标注册表、调度失准计数与板侧自描述（功能版）

> 交付日期：2026-10-09（终态提交日）｜ 执行者交付，输入 = `versions/v2.27/PLAN.md`（已批准-冻结，出口 A 功能版·全量档）+ `versions/v2.26/REVIEW-r2.md`（2026-10-08，结论 = 通过，0 阻塞项；其 §5/§6 = 本版 §1.1 全量输入）
> 首行声明（§6 预申报形态）：实际起点 = `fb7bbd9`（B1 开工入仓批：`versions/v2.26/REVIEW-r2.md` 入仓 + `versions/v2.27/PLAN.md` 冻结登记 + BACKLOG/ROADMAP 规划者预改（REQ-6/8/9 已纳入、REQ-5/7 未认领、REQ-1 已完成、M3 新建/M1 结案）——计划 §6 **预申报**，交付首行如实声明）；快照打于入仓批末笔，对账区间 `snap-2.27-start..HEAD` 为纯版本任务面。
> 本版评审记录：`versions/v2.27/REVIEW.md` **尚不存在**——阶段 4 由审核方（≠执行者）独立会话产出，本档为其输入；输入侧引用 = `versions/v2.26/REVIEW.md`（r1，有条件通过）与 `versions/v2.26/REVIEW-r2.md`（r2，通过，0 阻塞项）。
> 快照：`snap-2.27-start` = `fb7bbd9`（**≠ `tag v2.26` = `074759f`**，差一笔入仓批——预申报）；打快照同批 `gh run list --limit 3` 无新 run、`gh release list` 无新增（输出见 §7.6——AC-9 证据）。

## 1. 交付摘要

主题 = **板上自证与可观测性**（M3-①②③④）。P0 G0 勘误与条文批：`versions/v2.26/DELIVERY-r2.md` 五处按值勘误（`CO-6` verify 时点口径 + `CO-7` numstat/枚举/fail 计数/时点差面四处，修订登记放行）+ 两模板与 `templates/DELIVERY.md` 三处条文（门改动授权通道 / 偏差表固定列 / rN 按值标注纪律）+ `CO-8` rN 机检定位盲区条文化并入 v3 池 + ROADMAP **M3 新建 + M1 结案**（②③④ 移出阶段判据转常设挂账，逐条点名承接载体）。P1 库面功能批：新模块 **`debug/et_metrics`（第 35 模块）**——定容命名指标注册表（计数器/仪表 + 可选 `et_hist` 挂接，`format` 为唯一渲染源，库导出两 handler）；`sys/et_sched` **周期失准计数**（`et_task_t` 尾部追加两字段 + 三新函数 + 默认容差宏，六既有签名逐字不变、调度行为零改动）；**`AT+FEATURES`** 裁剪/特性自描述（32 开关逐行 0/1 + 版本三件，未启用如实报 0）。P2 板侧窗口批（G474，Owner 确认在位）：跨 **16 版**同步债一次清偿（板侧 `et_config.h` MINOR 10→27，`diff -rq` 七目录 + 两文件全 OK 实测）→ 构建 0 warning → 烧前 ELF 版本串机检 PASS → SWD 烧录 verified → 走单五项全绿，其中 **`AT+METRICS` 的 `loop_miss=2` 为失准计数的板上第一手数据**（实机记录 §17，证据八件落盘）。P3 常设批：基线滚动三件事（v2.26 归档 worktree 真生成 + BASE 切换 + 发版点复读）、数字回刷同批（用例数 497→520 八处 + 门两处、模块 34→35、开关 31→32、机制门行 347→370）、登记链收口、本档。四形态回归 **520/520/527/521**、公开面纯追加 **28 项 / 修改删除 0 项**、`docsync` 终态见行首声明、`sizecheck` 3/0。

## 2. AC 自查表

> 时点口径：除注明外，本表数值 = P3-5 定稿时点实测（终态提交上）；日期 = 终态提交日 2026-10-09。计数 = 行计数口径（`grep -c`），逐值标注（HC-8：diff 行数 / 非空行数 / 行计数 / 出现次数四口径不混用）。

| # | 结论 | 证据 | 任务 |
|---|---|---|---|
| **AC-1** | **通过** | `versions/v2.26/DELIVERY-r2.md` 原式 `grep -c "双点同值"` = **0**（判据串不含判据行自身字面——本行不含该字面）；四处逐值命中：numstat **`26+/0−`**（`git show --numstat 0e85331 -- tools/docsync.sh` 实测；非空 25 = 注释 10 + 代码 15 + 空行 1）/ 触碰文件全列 **14**（`git diff --name-only 074759f..ec15356` 行计数）/ **`fail=1`**（一条 [自指] 断言红、双诊断行——`git worktree` 于 `7c7b58f` 复跑实测 pass=344 fail=1）/ 时点差面标 **(b)/(b自排除)/(d)/token 三面**（主档 `--verify` @`ec15356` FAIL 点名四行陈旧锚）；更正注记行在位（§1/§2 受影响行/§4 引文注记/§5.3/§7.1/§7.4⑧/§7.6/§8 共八处标注）；修订登记 token 在该档登记行（短语行与 token 行分行，见 §7.3）；docref 对该档 = **缺声明行**（预期态，HC-4） | P0-1/P0-2 |
| **AC-2** | **通过** | 三处条文各 `grep -c` = **1**：`docs/开发计划模板.md` "评审条件清单点名的门新增/门面变更 = 允许面内" = 1 / `templates/DELIVERY.md` "计划面外授权（来源 = REVIEW-rN T-n）" = 1（第四列头 + 增强要素段以"偏差表第四列"指代，避免同串二次命中）/ 两模板 "rN/勘误轮声称按值标注口径" 各 = 1 + "rN 档存在时的机检定位切换" 各 = 1；追加形态逐值（配对判定 = 旧行须为新行严格前缀；`git show --numstat ba09e66 -- <四件>` = 1+/1−、3+/3−、3+/0−、7+/0−）= **rewritten 0**；严格前缀扩展行 = `docs/评审记录模板.md` **1**（:34「定稿后终态复测」行——原式作 :25 系行号滞后，本机 `grep -n` 实测 snap 态与终态同为 34）+ `templates/DELIVERY.md` **3**（偏差表列头行 + 分隔行 + 增强要素行）；纯插入行 = `docs/开发计划模板.md` **3** + `docs/v3-candidates.md` **7**（旧行零改动）；**[v2.27-r2 T-2 勘误，`CO-2(v2.27)`]** 原式只写扩展一对（引述加反引号 = 失准原式，非判据串：`prefix-extended 1`）、未点名 `templates/DELIVERY.md` 的 3 对扩展行；`ROADMAP.md` `^\*\*M3` = **1** 且 M1 行含"转常设挂账" = 1；BACKLOG "已纳入 v2.27" = **3** 行（REQ-6/8/9）+ REQ-8 含 31/34 数字注记；`tools/` 除 HC-1 授权面零 diff：`git diff --numstat snap-2.27-start..HEAD -- tools/` = `docsync.sh` + apidump.sh（BASE 两处 = `git diff --numstat -- tools/apidump.sh` 实测 2+/2−，[v2.27-r2 T-1 勘误，`CO-1(v2.27)`]）两件，`docref.sh`/`gatecheck.sh`/`sizecheck.sh`/`bench.c`/`*.py` **零 diff**（逐文件点名见 §7.6） | P0-3/P0-4/P0-5/P0-6 |
| **AC-3** | **通过** | `debug/et_metrics.h/.c` 存在且整体在 `#if ET_MODULE_METRICS` 门内；`apidump --diff` 对 v2.26 基线 = **新增 28 项 / 修改删除 0 项**（逐值分解 = `et_metrics_*` **15** 函数 + `et_metrics_t`/`et_metrics_slot_t` **2** 类型 + `ET_MODULE_METRICS`/`ET_METRICS_MAX`/`ET_METRICS_KEY_MAX`/`ET_METRIC_COUNTER`/`ET_METRIC_GAUGE` **5** 宏 + `et_features_format`/`et_features_cmd` **2** features 面 + `et_sched_task_miss`/`et_sched_task_miss_reset`/`et_sched_task_set_tolerance` **3** sched 函数 + `ET_SCHED_MISS_TOL_MS` **1** 容差宏 = **28**（分解和 = 总数自洽）；**[v2.27-r2 T-2 勘误，`CO-2(v2.27)`]** 原式函数数失准（`et_features_*` 两函数不属 `et_metrics_*` 前缀）、且原分解和 17+2+5+2 = 26 ≠ 28（sched 面仅作跨行指称未计入分解）——总数 28 与「公开面无漏登」结论不变）；`et_metrics_format`/`et_features_format` 的 host 逐字断言在 `test_metrics_cases`（`metrics.format_verbatim` 逐字节比对 + `metrics.features_*`）且 `make test` 全绿；五面文档义务逐条机读命中：README 特性表行（表实测 **35** 模块行）/ API_GUIDE `### 8.5 et_metrics` + 目录项 + `## 10` 四行配置 + `### 11.15` / architecture 分层框 `:13` + 选型表 `:104`/`:105` / `Makefile:44 DEBUG_SRC` + `TEST_SRC` / `test_main.c` extern + `g_suites` 各一处；docsync 新模块断言块 **K = 22 行**实测在位（含特性表行数↔开关数对账断言，实测 32 = 32 绿） | P1-1 |
| **AC-4** | **通过** | `et_task_t` 尾部两字段追加（`sys/et_sched.h` `miss_cnt`/`tol_ms`，注释沿用 v2.2 追加标记形态）+ `docs/API_STABILITY.md` §5.1 新登记行 = **1**（含 `sizeof` 16B→24B 半径与"不经 register 直接使用"的行为侧评估）；`et_sched_register/_poll_once/_next_due/_reset/task_stats/task_stats_reset` **六签名逐字不变**（`git diff snap-2.27-start..HEAD -- sys/et_sched.h` 对照，新增行仅尾部声明块与头注）；三新函数存在且 miss 用例以虚拟时基确定性通过（`sched.miss_exact_zero`/`miss_in_tolerance`/`miss_beyond_counted`/`miss_stall_once`/`miss_reset_tol`/`miss_reregister`/`miss_null_safety` 七例——含"≤ period+tol 不计、> 计"与"重锚后不误计"两类）；API_GUIDE `### 4.2` 含主循环停转漏检的已知边界声明（§4.2 表新增三行 + 口径段四条） | P1-2 |
| **AC-5** | **通过** | `et_features_format` 输出 = `FEATURES v2.27.0 (maj=2 min=27 pat=0) sw=32` + **32 个开关逐行 0/1**（行计数口径：`grep -c` 值行 = 32，总行数 33 = 表头 1 + 32）；"未启用如实报 0"以**二形态同时承载**：① 表值断言（`metrics.features_table_tracks_macros` 按 `(ET_MODULE_x ? '1':'0')` 同式求值比对，证非硬编码）；② 显式关闭构建变体（`build/ac5-features-variant-stats0.txt`：`-DET_MODULE_STATS=0 -DET_MODULE_SELFTEST=0` 变体构建 → `STATS=0`/`SELFTEST=0` 两行在档且 `lines=33` 不变；对照默认构型 `SELFTEST=0` 亦如实报 0——发布默认裁剪态）；库不持有命令表：`protocol/et_atcmd.*` 零 diff（§7.6 逐文件点名） | P1-3 |
| **AC-6** | **通过** | `移植stm32实机记录.md` **§17** 在档（`grep -n '^## '` 实测节号 17，开工时 §16 为末节、§17 未占用）且含五项走单原文读数：`AT+VER` → `ver=2.27.0 boot=30` / `SELFTEST: 22/22 PASS` / `AT+MBRD 0 4` **三形态（CRLF/仅CR/仅LF）逐行 `disc=0`**（`MBSTAT req=1/2/3 resp=1/2/3 … disc=0`）/ `AT+FEATURES` 32 开关逐行 / `AT+METRICS` 含 **`loop_miss=2`**；`diff -rq` 同步实测登记在 §17.1（七目录 + 两文件全 OK，板侧 MINOR 10→27 = 跨 16 版债，**实测非凭记录**）；证据目录 `D:\code\STM32CubeMX\G474VET6_ET_TEST\build\v2.27-evidence\` **八件**在位（`diff_rq.txt`/`sync_sha256.txt`(72 条 sha256)/`build.log`/`version_check.txt`/`ports.txt`/`flash.log`/`walk_output.txt`/`walk_summary.md`；驱动 `walk.py` 不入库沿 §16.4 形态）；`port/stm32g474/README.md` 原式 `grep -c "v2.10.0 同步态"` = **0**（16 行历史声称改写为"该版时点未重烧（落差自 v2.10 窗口起累积，v2.27 §17 一次清偿）"形态，历史语义保留）且新声称与 §17 同值（v2.27 行）；烧前机检留痕在案（`version_check.txt` 含工具链串干扰项排除说明，`['13.3.1','2.27.0','2.42.0','4.4.0']` 集合断言 PASS 后才烧录） | P2 板侧窗口 |
| **AC-7** | **通过** | `docs/API_INVENTORY_v2.26.md` 存在（`git worktree` 于 **tag v2.26 = `074759f`** 用当版 `apidump --snapshot` 真生成，**433 项**，禁手工编辑）；**blob 层 sha256 = `694ddfd03d242b6b04dee8c5098e4a8e697de37ad516eb2fc5b5d90c608e4a98`**（`git hash-object` = 72651529… 同对象，`git cat-file blob` 复跑逐位一致）；与 v2.25 归档（blob sha256 `7db21135…`）blob 层 diff **逐行 = 仅 `ET_VERSION_MINOR` 一行**（25→26，1 删 1 增；`diff` 命中 2 行 = 1删1增；条目数 433 不变 = 公开面零变化预期命中，无超出）；`item_lines` 口径值实测 = **433**；`tools/apidump.sh` BASE = v2.26（`:15` 头注 + `:227` 默认值两处实测 + 履历续写）；发版点 `--diff` 复读见 §7.6（修改/删除 0，新增项与 AC-3 点名清单一致） | P3-1 |
| **AC-8** | **通过** | HC-5 清单逐项命中：MINOR **27** / `0x021B00`（`gcc -E -P -dM et_config.h \| grep ET_VERSION` = 2/27/0；docsync 整数编码断言同值）；README 版本行 + 交付链接行（→ `versions/v2.27/DELIVERY.md`）+ 基线名前滚 ×2（`:194`/`:239` → v2.26 归档名）；**用例数 520 = 文档八处 + 门两处同值**（八处 = `docs/getting-started.md:40`、`README.md:60/:182/:186`、`docs/architecture.md:112`、`docs/API_GUIDE.md:1453/:1466/:1514`；门两处 = `tools/docsync.sh:289/:291`；五处一致断言 `docsync.sh:458-474` 绿；**另两处** = 两 port README host 读数行，计划清单未列、按 README 清单① 同批改写——偏差 2）；模块数 **35**（architecture `:74` + README 特性表实测 35 行 + CHANGELOG 第 35 模块）/ 开关数 **32**（`grep -c '^#define ET_MODULE_' et_config.h` = 32，且 docsync 断言"AT+FEATURES 表行数 == 开关数"实测 32 = 32 绿）；CHANGELOG `[2.27.0]` 日期 = **2026-10-09** = 终态提交日；architecture 机制门行 = **370**（复算推导见 §7.1，与终态实测一致） | P3-2 |
| **AC-9** | **通过** | 四形态读数实测 **520/520/527/521**（default/g4/Tab/1K，各形态 fail=0；基线 497/497/504/498 @v2.26 → **各形态 +23**（`520−497 = 520−497 = 527−504 = 521−498 = 23`；metrics 16 例 + sched 失准 7 例 = 23，与本档 §7.6「新增 23 例」同值；口径 = `[Suite] pass=` 逐套件求和行计数）；**[v2.27-r2 T-2 勘误，`CO-2(v2.27)`]** 原式作区间形态（引述加反引号 = 失准原式，非判据串：`+21~23`），终态四形态差值实测各 = 23）；**[v2.27-r2 R9 勘误，`CO-11(v2.27)`]** 本行 r2 T-2 改写自身引入两处文本形态缺陷——外层全角括号少一处闭合（改前行 7 开 6 闭）、「各 = 23」与下一声称以加号相接（可读成「23 以上」）；本笔严格 1:1 行内补闭合与分隔符（该行列数与本档全文行数不变、数值面零变化）；`mingw32-make ex` 五配方 **PASS ×5**（留痕 `build/ex-v227.log`）；`sh tools/docsync.sh` = **pass 370 fail 0**（§7.1，行首声明对账）；`sh tools/apidump.sh --check` exit **0**；`sh tools/sizecheck.sh` = **pass=3 fail=0**（f103 25600/24/1512 与 38044/24/3792、g474 25976/28/1512 全部实测回填，体积增量归因 = 新模块无 gc-sections 全量入 ELF）；`make WERROR=1` 零警告构建复跑在案；CI 七 job / Release 四附件 / `gh` 快照时点无 `snap-*` 误触发见 §7.6（HC-2 实测） | P3-5 |
| **AC-10** | **通过** | §7.4 行⑧ 五锚行 = `sh tools/gatecheck.sh 2.27 --emit` **逐字粘贴**（引用纪律，禁重打）；`--verify` **PASS @ 定稿链末笔**（§7.4 行⑧ verify 对账段）；tag 后 verify 红 = 仅 (d) 锚预期差且与 §7.5 逐字吻合；token 机读与 §7.3 一致；**(d) 全集 = 31 + 1 = 32 份**（算式：平铺 28 + `versions/v2.25` + `versions/v2.26` 两档 + 本档 = 32；分类值以发版点机读输出为准逐字粘贴，禁预抄）；本档 = 见 §7.4⑧ 片段实测分类 | P3-4/P3-5 |
| **AC-11** | **通过** | G0 窗口谓词（§3 P0 逐文件枚举：`et_config.h` + 库面六目录 `core/ algorithm/ sys/ protocol/ drivers/ storage/` + `Makefile` + `tools/{docref,gatecheck,apidump,sizecheck}.sh`）于 **G0 终态提交 `ba09e66`**（C0 末笔，右端时点化）实测 = **空**（`git diff --name-only snap-2.27-start..ba09e66 -- …` 零行）；`tools/docsync.sh` 显式排除并归因（本版 P1-1 断言块 + P3-2 数字面 = 计划内，非 G0 面）；G0 触碰面实测非空 = `versions/v2.26/DELIVERY-r2.md` + 两模板 + `templates/DELIVERY.md` + `docs/v3-candidates.md` + `ROADMAP.md` 五类 6 files（26+/14− numstat；`BACKLOG.md` 面在入仓批 `fb7bbd9`）；docsync G0 终态 = **347/0** 复算值（G0 零新增断言，P0-3/P0-4 全为文档面）；docref 对 `DELIVERY-r2.md` = 缺声明行（预期态） | G0 窗口 |
| **AC-12** | **通过** | `sys/et_sched.c` 与 `sys/et_sched.h` 含 `et_metrics.h` 的 `grep -c` 均 = **0**（并入门控：`tools/docsync.sh` 新增 `assert_no_grep "sys/et_sched.c" "et_metrics.h"` 断言，红即拦截反向依赖）；`core/` `algorithm/` 含 `#include "port.h"` 的实测命中文件 = **0**（既有不变式复测）；`debug/et_metrics.h` 的并发标注逐 API 显式在档 = `🔒ISR-safe` **4** 处 + `🏠MAIN` **12** 处（行计数口径）；零动态内存字面：`grep -c "malloc\|calloc\|realloc\|free" debug/et_metrics.c debug/et_metrics.h` = **0 / 0**；失准计数↔注册表的联动只在板侧私有 demo（库外 `Core\Src\et_demo.c` 心跳任务读 `et_sched_task_miss` 再写命名指标）完成 | HC-9 |

## 3. 偏差记录

| # | 偏差 | 说明 | 处置 | 计划面外授权（来源 = REVIEW-rN T-n） |
|---|---|---|---|---|
| 1 | **实际起点 = 入仓批 `fb7bbd9` ≠ 计划基线 `074759f`** | 计划 §6 **预申报**形态（v2.26 偏差 3 同型）：开工前未入库件 = `versions/v2.26/REVIEW-r2.md` + `versions/v2.27/PLAN.md` + BACKLOG/ROADMAP 规划者预改（P0-5/P0-6），作首笔入仓批；`snap-2.27-start` 打于该批末笔 | 首行声明 + 本行留痕；对账区间 `snap-2.27-start..` 为纯版本任务面 | 无 |
| 2 | **数字回刷面宽于计划八处清单（两 port README host 读数行）** | HC-5 列"用例数 497 系回刷面 = 实测 8 处 + 门两处"；实测另有 `port/stm32f103/README.md:169`、`port/stm32g474/README.md:119` 两处携带同一读数（同一提交改了同文件的体积行，不改即成失实文本，且 README 清单①"无断言的同步声明视为未同步"） | 同批改写为 520/521/527 并在此登记（范围外改动 = 2 文件 2 行，纯数值同步，零判定语义影响） | 无 |
| 3 | **AC-6 判据宽于 P2-4 任务面（16 行 vs 点名 2 行）** | 计划 P2-4 只点名 `port/stm32g474/README.md:103/:104` 两行"维持 v2.10.0 同步态"声称，而 AC-6 判据 = 该原式 **全文件 grep -c = 0**；实测该原式在 v2.11–v2.26 共 **16 行**体积表声称中存在——判据与任务面在任何时点均不可同时满足（模板"判据自洽性检查"同族） | 按最合理方式继续：**16 行全部改写声称形态**（"该版时点未重烧（库/板落差自 v2.10 窗口起累积，v2.27 §17 一次清偿）"），历史语义与时点态保留、不改写数值与结论；偏差登记并转 §5 建议（下版计划任务面应显式"全文件穷举"） | 无 |
| 4 | **`et_metrics` 命令面新增公开函数 `et_metrics_bind()`（计划 API 形状清单未列）** | 计划 P1-1 的 handler 形态 = `et_metrics_dump_cmd(char *args, void *user)`，而 `user` 已被 atcmd 用于 `et_shell_t*`（沿 `{"HELP", et_shell_help_cmd}` 先例，实测 `debug/et_shell.c:387`）——dump 命令无从得知读哪一份注册表；实现采单静态绑定面（先例 = `sys/et_sched.c` 文件静态任务表），库内只存**只读句柄**、存储仍归调用方 | 计入 HC-6 点名清单（`apidump --diff` 新增 28 项含本函数）；可裁量① 存储/形态取向内决策，公开面纯追加零破坏；API_GUIDE §8.5 条文写明"多实例场景经包装函数直调 `et_metrics_format`" | 无 |
| 5 | **P3-1①②（归档 + BASE 切换）与 P3-2(a) 合批提前** | 计划 §6 顺序把 P3-1 排在板侧窗口之后，但 `docsync.sh` 基线名交叉断言（`:501-512`）强制 README 基线名 = `apidump` 默认基线 = 最新归档三者同值——`（a）` 段既含"基线名前滚 ×2"，归档与 BASE 必须同批，否则 G0 后第一提交即门红 | 提前合批（提交 `df56eaf`），P3-1③ 发版点复读仍按计划在定稿链执行；顺序调整零范围外改动 | 无 |
| 6 | **AT+FEATURES 表形态 = 编译期宏常量折叠，非逐开关 `#if`** | 计划 P1-3 原文"编译期 `#if` 生成的静态描述表"；实现用 `ET_FEAT(UPPER)` 宏拼接 `#UPPER` + `(ET_MODULE_##UPPER ? 1u : 0u)`，逐行值由编译器常量折叠（等价语义：32 行静态表、未启用如实报 0 不省略行；若逐开关写 `#if` 则新增开关时**忘写 `#else` 分支会静默省略该行**，反成失实面） | AC-5 以表值断言 + 变体构建双形态证语义等价；配套加门（docsync 断言"表行数 == `et_config.h` 开关数"）——形态登记于此，非行为偏离 | 无 |
| 7 | **板侧烧录首次连接报 `No debug probe detected`（瞬态，重试即成功）** | 探针枚举面实测 `STM32 STLink`（`USB\VID_0483&PID_3748`，Status=OK）在位；同命令第二次执行即连（SN 52FF6C067087535043161267 / FW V2J47S7 / 3.23V / Device ID 0x469）并烧录 verified | 实机记录 §17.1 如实登记该瞬态，不写成"一次通过"；无库面影响、无走单失真（后续读数均取自同一次会话） | 无 |
| 8 | **历史档登记行面（七档各 +1 行）与冻结计划 §1.3/HC-1/HC-4 字面冲突** | 计划 §1.3 写「v1.1~v2.25 平铺档与 `versions/v2.25/` 零触碰」、HC-1 文档面枚举未列该七档、HC-4 把放行面写作「一处档面」（仅 `versions/v2.26/DELIVERY-r2.md`），而 P3-3 常设登记义务 + docref 白名单判定（(d) 桶「未登记实质变更」计数）强制非白名单点名面须落各 OK 档登记行 = **冻结计划内部矛盾**（v2.26 计划 §1.3 曾写 carve-out「仅 §6 回填 + 登记行」，本版漏写）。C6 实触碰面逐档 = `v2.20开发交付__定稿后复测机制与基线口径明示.md` / `v2.21开发交付__勘误收敛与封闭枚举.md` / `v2.22开发交付__非代码清欠与复测脚本化.md` / `v2.23开发交付__引用机检闭环与纪律块重组.md` / `v2.24开发交付__勘误收敛与verify时点固化.md` / `versions/v2.25/DELIVERY.md` / `versions/v2.26/DELIVERY.md` 各 1+/0−（对账命令 = `git diff --numstat snap-2.27-start..HEAD -- <七件>` 与 `git diff --name-only` 行计数 = 7） | 按评审 §4 末行裁决补记（需补记、非返工 = `CO-4(v2.27)` 交付侧，r2 T-4）；根因在计划侧 ⇒ 登记行 carve-out 与 HC-1/HC-6 交叉引用转 **v2.28 计划必含项**（评审 §6 已列 CO-4/CO-5 去向）；r2 轮自身触碰同七档 = 纯登记豁免面（T-1 勘误行 + 修订登记 token 收口行） | 无（门强制面 ≠ 授权面：docref (d) 桶 + P3-3 常设义务驱动，计划文本矛盾由 CO-4 承载） |

> 列形态注记（**[v2.27-r2 T-3，`CO-3(v2.27)`]**）：本表按 `templates/DELIVERY.md` 固定列形态补第五列「计划面外授权（来源 = REVIEW-rN T-n）」，8 行各值 = **无**——本版门改动（`tools/docsync.sh` K=22 断言块 + `tools/apidump.sh` BASE 滚动）全在计划 HC-7 显式授权面内，不存在评审条件清单驱动的门新增/门面变更；该列同时承载 diff 行数 / fail 计数 / 文件枚举 / 时点差覆盖面类声称的口径标注（`CO-7(v2.26)` 同族），本版此类声称的口径一律在 §2 证据列逐值写出（偏差 2「2 文件 2 行」= numstat 口径、偏差 3「16 行」= 全文件 grep 行计数口径）。

## 4. commit 范围

记录范围：**`snap-2.27-start`（= `fb7bbd9` = 入仓批）`..v2.27`**（终点冻结为 tag 符号 `v2.27`）。

`git diff --stat snap-2.27-start..v2.27` = **36 files changed, 2770 insertions(+), 64 deletions(-)**（占位形态：本行由定稿链 C7b **1:1 回填**实测三数，行数不变——⑧ (b) 两口径随之稳定；口径 = 全量含本文档，自排除并列值见 §7.4⑧ 片段行）

```
（本提交 = C7b）v2.27 P3-5(C7b): 定稿链末笔——⑧ 五锚行与 §4 三数/C7a SHA 1:1 回填 + verify 对账与终态门批次留痕
3b96494 v2.27 P3-5(C7a): §4 声明行形态归位（占位三数）+ 定稿链两笔入清单（本笔起行数与 (b) 稳定，C7b 严格 1:1）
682d02f v2.27 P3-3(C6): 登记链收口(纯登记)——七档各 +1 登记行（docref 实测点名面, 复测 OK×7）+ DELIVERY-r2.md 修订登记 token
4a59ef1 v2.27 P3-4(C5): versions/v2.27/DELIVERY.md 落仓 + v3-candidates「v2.27 复评记录」节(17 行同源) + CHANGELOG 日期校正
792f0bd v2.27 P3-2(b)(C4): 数字回刷其余数值段
56d05d8 v2.27 P2(C3): G474 板侧窗口批
df56eaf v2.27 P3-2(a)+P3-1①②(C2): 版本宏段回刷与基线滚动
fe0f58d v2.27 P1(C1): 库面功能批
ba09e66 v2.27 P0(C0): G0 勘误与条文批
fb7bbd9 v2.27 B1 开工入仓（= snap-2.27-start，预申报见首行）
```

（清单口径与排除注记：**对账基准 = `git log --oneline snap-2.27-start..v2.27`**，其中本清单列版本任务面各笔 + 定稿链笔；本档提交（C5）与定稿链（C7 = 本提交）按 docref「清单不含交付文档定稿提交自身」同源自指纪律以标记承载，SHA 于 1:1 回填时实测；C6 = 登记收口笔（纯登记豁免面）。触碰文件全列与各笔归因逐笔见 §1/§7.3——**点名任务面**与**全列**两口径按值标注（HC-8）。）

## 5. 未完成项与新发现

1. **计划任务打勾形态**：`versions/v2.27/PLAN.md` §3 为无复选框列任务列表——完成情况以本表 §2 AC 自查表逐任务承载（AC ↔ 任务号映射），沿 v2.26 交付 §5.1 先例（未擅自向冻结计划加列）。
2. **ROADMAP 计划状态行校正**：规划者预改件（入仓批）中 `ROADMAP.md` 项目状态行写作"待批准冻结"，而 PLAN 头部终态 = **已批准-冻结**（Owner standing 规则，PLAN 变更记录第二行在案）——随 P0 批按 PLAN 终态校正，未新增语义。
3. **新发现（顺手不修，登记面）**：`docsync.sh` 的 [自指] 断言在中间态以 **1 条断言、2 诊断行**形态报红（architecture 与交付声明各处一行）——即 `CO-7(v2.26)` 所述"fail 计数 vs 诊断行数"混用的机制源在门内；本版按新纪律在 §2/§7.1 逐值标注，不改门（属判定语义变化，随 v3 窗口）。触发 = v3 池内既有的"⑧/doc↔log 常设化"评估同批。
4. **新发现**：`apidump --diff` 的"新增项"清单不含**宏值变更以外的开关语义**——`ET_MODULE_*` 开关新增会同时改变 `AT+FEATURES` 面与 `## 10` 配置表，三处需人工同批；本版以 docsync 断言"表行数 == 开关数"闭一半，另一半（配置表枚举行）无机检。建议入池：`API_GUIDE ## 10` 开关枚举行与 `et_config.h` 开关集合的机械对账断言。新功能/新场景类 = 无（未写入 BACKLOG 速记区，本条属机制建议，随下次判定处置）。
5. **REQ-5 / REQ-7 未认领（维持已接受）**：本版 G474 窗口留下可复现的手工基线流程与逐项读数形态（§17.1 表 + 八件证据），即 REQ-5 脚本化的对照真值；REQ-7（真固件 IAP 走单常设化）触发随 REQ-5 联动，本版窗口为 SWD 直烧，未涉 IAP 路径。ch32x035 = COM16/WCH-Link 本次枚举不在位 → 挂账续期（§17.4 + 常设条目 v2.27 行）。

## 6. 遗留风险

- **板侧同步债已清偿但义务为常设**：`docs/v3-candidates.md`「G474 板侧同步窗口义务」（PF-1）自此承载逐版状态列；风险 = 下版若改库面而不跑窗口，债重新累积（v2.11–v2.26 十六版即此形态）。触发 = 每版改库面即窗口义务（已写入条目）。
- **ch32x035 上板回归债 / f103 真机**：维持挂账（常设结构化条目 v2.27 行在档；触发 = WCH-Link 枚举在位 / 补板或 Renode 深化判据）；本版板侧窗口 = **仅 G474**。
- **登记链 token 膨胀**：本版触碰非白名单面 = `tools/docsync.sh` + `tools/apidump.sh` + 库面 + `et_config.h` + `Makefile` + 两 port README + 实机记录 + `versions/v2.26/DELIVERY-r2.md` + BACKLOG/ROADMAP，各 OK 档登记行随之增 token（终态值以 §7.4⑧ 片段机读为准，禁预抄）——链并行维护面继续扩大，载体形态评估维持 v3 池。
- **板上 `et_metrics` 命令面缓冲**：`g_io_buf` = 768 B（.bss +772 实测含只读句柄）——port demo 未调用亦全量入 ELF（本 port 构建无 gc-sections，沿 v2.1 行同因口径）；内存紧的移植应 `ET_MODULE_METRICS=0` 裁剪或自带更小缓冲直调 `format`。
- **`AT+FEATURES` 报的是编译期开关值**：不是"运行时链接了否"；`SELFTEST=0`（库内发布默认）与板侧 `SELFTEST=1`（构建 `-D` 启用）并存属如实上报，现场误读风险以 API_GUIDE §8.5 边界条注明。
- **失准计数的已知边界**：主循环停转期间的漏检不逐周期计（一次补跑至多 +1），与"不补跑积压"的调度策略一致——`AT+METRICS` 的 `loop_miss` 回答"有没有失准/几次"，不回答"错过几个周期"（API_GUIDE §4.2 口径四条在档）。
- **verify 时点与 rN 定位**：verify 绿态 = 打 tag 前定稿链末笔单点（P1-2 口径，本版按 `CO-6(v2.26)` 更正后的表述写，不写"双点同值"）；本版无 rN 轮，若后续产生 `DELIVERY-r2.md`，`[自指]`/docref 无参定位对象按 P0-4 条文 = `head -1` 命中的 rN 档（主档声明不再被机检）。

## 7. 机制与证据区

### 7.1 docsync 计数推导（行首声明）

> 本版 docsync：**370/370**

N 复算推导（计划预置公式 `N = 上版实测 + 当版交付文档数 + 当版 rN 文件数 + 1（CHANGELOG 链）+ K`）：上版终态 **347**（`ec15356`/`fb7bbd9` 时点实测）+ **K = 22**（P1-1/P1-2/P1-3 新模块断言块，含特性表行数↔开关数对账；**K 以 diff 实测**——`git diff --numstat snap-2.27-start..HEAD -- tools/docsync.sh` 的净增行含 P3-2 两处数值面改写不计数，计数以断言条数为准）= 369（C1 后实测）→ 版本宏回刷使「占位符拦截」循环从 v2.26 两份切至当版零份 = 367 → CHANGELOG `[2.27.0]` 入版本链循环 +1 = 368（C2/C3/C4 时点实测 pass=366 fail=2，两红 = 缺当版交付文档的计划性时点态）→ 本档入覆盖率行循环 +1 = 369 → 本档入占位符拦截循环 +1 = **370**（逐步无滑差）。architecture 机制门行同批回填 **370**（P1-1 门改动三件套 = 回填 + N 复算 + docref 登记，即 `CO-9(v2.26)` 条文的首个实例）；终态实测 = pass 370 fail 0（定稿链复跑，§7.6）。中间态实证（两笔时点，如实登记）：C1 后 = 实测 369（architecture 预置 370 ≠ 369 = 计划性时点态，当版交付文档未落仓）；C2–C4 = 368（366 pass + 2 fail，两红 = 缺当版交付文档）。

### 7.3 登记链

| 提交 | SHA | 内容 | 对既有 OK 档登记面 |
|---|---|---|---|
| 入仓批 | `fb7bbd9` | versions/v2.26/REVIEW-r2.md + versions/v2.27/PLAN.md + BACKLOG/ROADMAP 规划者预改（快照前，预申报） | 非良性（BACKLOG/ROADMAP 非白名单；REVIEW/PLAN 为白名单面）→ 各 OK 档登记行收录（C6） |
| C0 | `ba09e66` | P0 G0 勘误与条文批（DELIVERY-r2.md 五处 + 两模板 + templates/DELIVERY + v3-candidates + ROADMAP） | 半良性：两模板/v3-candidates 属白名单；`versions/v2.26/DELIVERY-r2.md`（触及该档本体）+ `ROADMAP.md`（非白名单）→ 点名面由 C6 收录 |
| C1 | `fe0f58d` | 库面功能批（新模块 + sched 追加 + et_config + Makefile + 测试 + 四文档 + `tools/docsync.sh` K=22） | **非良性**（`tools/docsync.sh` + 库面）→ 各 OK 档登记行收录（C6） |
| C2 | `df56eaf` | 版本宏段回刷 + v2.26 归档 + `tools/apidump.sh` BASE | **非良性**（`tools/apidump.sh` + `et_config.h` + CHANGELOG）→ 收录（C6） |
| C3 | `56d05d8` | G474 板侧窗口批（实机记录 §17 + 两 port README + v3-candidates PF-1/ch32x035 行） | 半良性：v3-candidates/两 port 属点名面，`移植stm32实机记录.md` 非白名单 → 收录（C6） |
| C4 | `792f0bd` | 数字回刷其余数值段（含 `tools/docsync.sh` 两处数值面） | **非良性**（docsync 数值面）→ 收录（C6） |
| C5 | （本档提交，SHA 由定稿链 1:1 回填） | versions/v2.27/DELIVERY.md + v3-candidates「v2.27 复评记录」节 | 良性（`versions/*/DELIVERY*` 与 `docs/v3-candidates.md` 白名单） |
| C6 | （登记收口笔） | 各 OK 档登记行纯登记收口 | 纯登记豁免 |
| 定稿链 | P3-5 | ⑧ 片段 1:1 回填 + verify 留痕 + §4 SHA 回填 | 良性（仅触及本档；不自列，§4 注记口径） |

登记 token 终态（gatecheck token 机读，定稿时点）：见 §7.4⑧ 片段"登记 token"行逐字承载（禁重打、禁预抄）。docref 口径发版点复验：对既有 OK 档逐档 = OK exit 0（点名面以实测为准，§7.4⑧ (d) 片段行承载分类值）；本档 = tag 后经典路径 OK（§7.5 复现预期）。
- 修订登记（短语行不携 token，遵碰撞规避纪律；token 行另起）：本档对 `versions/v2.26/DELIVERY-r2.md` 的五处按值勘误（P0-1/P0-2，`CO-6`/`CO-7`）走放行
- 修订登记（r2 轮，短语行不携 token）：v2.27-r2 评审条件批对本档 §2/§3/§8 的实质变更与本轮门强制数值面走放行，来源 = `versions/v2.27/REVIEW.md` §6 条件清单 T-1…T-4（载体 = `versions/v2.27/DELIVERY-r2.md`）
- 修订登记 @f3888d9 @65a385b @b0c0a18 @b27d78c：r2 批逐笔点名面 = R2（T-1 十处字面 / 九档 1:1 行内替换，含本档 AC-2 与 §8 末段）/ R3（T-2 本档 §2 三格逐值改写）/ R4（T-3 列形态 + T-4 偏差 8 补记 + §8 CO-9 行更正）/ R5（门强制 `docs/architecture.md` 机制门行 370→372，非白名单）；R1 评审记录入仓与 R6 纯登记 = 白名单/豁免面，不点名
- 修订登记（R9 勘误轮 + 发版批，短语行不携 token）：v2.27-r2 R9 对本档 §2 AC-9 行的严格 1:1 文本形态修复（`CO-11(v2.27)` 清偿）与 `versions/v2.27/REVIEW-r2.md` 复评记录入仓走放行；来源 = 复评 §5 二择一之①「发版前补 R9 一笔」+ 同批发版步（Owner 2026-10-10 授权执行者代做 merge/tag/push/CI/Release，归因另记 `versions/v2.28/PLAN.md` 变更记录，本档不代 Owner 改 REQ 状态位）
- 修订登记 @fc5b719：R9 = 本档 AC-9 行外层全角括号补闭合与「各 = 23」接缝分隔符修复（numstat 1+/1−、行数 174→174 不变 ⇒ (b) 双口径锚值提交后实测逐值命中 38/3170/64 与 36/2830/64；`docsync` 372/0 同值；`gatecheck 2.27 --verify` 对 r2 档 exit 0 同值绿）——纯文本形态面，零声称数值变化。本登记行为发版批所加（一笔不能携自身 SHA），故本行自身与复评记录入仓笔不在列；该两笔之后 (b) 区间因新增文件增长 ⇒ `--verify` 转红 = 预期态（`versions/v2.27/DELIVERY-r2.md` §6 末条 + 复评 §6 项 3 口径）

### 7.4 ⑧ 自检表（引用机检第六周期）

| 检查 | 本文档执行 | 实测 |
|---|---|---|
| ① 判据 vs 任务自洽 | G0 窗口谓词逐文件枚举（`et_config.h` + 库面六目录 + `Makefile` + `tools/{docref,gatecheck,apidump,sizecheck}.sh`），`docsync.sh` 显式排除并归因（本版 P1-1/P3-2 计划内）；不设裸目录谓词、不设 docs 空谓词（P0 批必改两模板与 v3-candidates） | 本表写作时核对 ✓ |
| ② 右端核对（与自称终态一致） | AC-11 G0 窗口右端 = `ba09e66`（时点化，实测回填）；§4 右端 = tag 符号 `v2.27`（终点冻结口径，tag 前结构性不可测 = (d) 其他类分段形态，复现预期见 §7.5）；⑧ (b) 右端 = 版本键区间 `snap-2.27-start..HEAD` | 双侧证据机械可核 ✓ |
| ③ 预置数值逐项代入 | N = 370 复算六步在案（§7.1，含 K=22 实测与占位符面随版本切换的 −2/+1 缺项——计划预置公式未列"占位符拦截面"一项，按实测补正，见 §7.1 括注）；体积行 `arm-none-eabi-size` 实测回填（sizecheck 3/0）；用例数 520 四读数实测；(d) 算式 31 + 1 = 32 | 终态实测 = 370 ✓ |
| ④ 判据串不自匹配 + 与规定输出形态自洽 | 本表判据作用域不含本文档自身（AC-1/AC-2 grep 目标 = `versions/v2.26/DELIVERY-r2.md` 与两模板；AC-4/AC-12 = 库面文件；AC-5 = `build/ac5-*.txt` 留痕；AC-6 = 实机记录/板侧证据目录/port README）；AC-1 的原式判据"双点同值"由本档转述承载、不含该字面（判据行不自命中）；⑧ 片段为既定输出形态，引用时显式排除于散文计数外 | 见本表 ✓ |
| ⑤ 时点限定 + 引证 `ls` + 跨文档引用完整形态 | 时点限定见 §2 首注、§7.1/§7.3/§7.5/§7.6/§7.7 各注（日期 = 终态提交日 2026-10-09）；跨文档引用完整形态（`versions/v2.26/REVIEW-r2.md` / `versions/v2.26/DELIVERY-r2.md` / `versions/v2.27/PLAN.md` / `docs/v3-candidates.md` 等全名）；引证文件逐个 `ls`/`grep` 命中（本次交付会话内实测；板侧八件证据为库外绝对路径） | ✓ |
| ⑥ 按值声明口径 + 机械抽检 | §2 全部计数值按值标注口径（行计数 / numstat 增删 / fail 计数 vs 诊断行数 / 出现次数分列）；机械抽检（对 ≥2 声称值抽一条复跑）：抽 AC-5 的"开关逐行 32 行"复跑 `grep -c '^  [A-Z_]*=[01]$' <默认构型渲染件>` → 实测 = **32**（命令与输出即本单元格；终点锚为行首形态，落笔前实测命中，沿「追加即复测」纪律——§5 三处新节追加后本抽检复测仍 = 32，不随文件增长漂移） | 抽检通过 ✓ |
| ⑦ 仓库状态类声称收口 | §2 各值注明时点（P3-5 定稿时点或逐项明示）；AC-3/AC-5/AC-7/AC-8/AC-12 的计数、§7.5 全集、§7.7 归档 sha256、§2 AC-6 板侧读数均为明示时点实测 | 逐条核对 ✓ |
| ⑧ **定稿后终态复测（封闭枚举——引用机检第六周期）** | **取值命令原文 = `sh tools/gatecheck.sh 2.27 --emit`**（片段由 --emit 逐字生成粘贴，禁重打；时点 = 定稿链末笔）。**(b) 类**：全量 / 自排除 双口径以片段行承载（含本档，自排除口径并列）。**(d) 类**：全集逐文档 docref 判定（片段行 + §7.5 明细；本档分类以机读为准）。**(a) 类全局项**：CHANGELOG `[2.27.0]` 节日期 = 2026-10-09（终态提交日 ✓）+ 全文日期计数随片段。** (c) 类**：G0 窗口 = AC-11 谓词实测（右端 `ba09e66`）= **空**，人工命令原文已在 §2 AC-11 行写明。**verify 对账**：`sh tools/gatecheck.sh 2.27 --verify versions/v2.27/DELIVERY.md` → **verify PASS exit 0**（绿态单点 = 本档 ⑧ 片段粘贴所在定稿链末笔；留痕 `build/final-gates-v227.log`——`build/` 属 gitignore 面不可入仓，故以本行措辞承载实测，沿 `CO-6(v2.26)` 更正后的口径书写；红即不放行 tag；tag 后红 = 预期态——P1-2 固化口径，与 §7.5 逐字对账）。**四类齐备——本表 (a)(b)(c)(d) 四段齐备** | 四类齐备 ✓ |

**§7.4 行⑧ 机读值片段（`sh tools/gatecheck.sh 2.27 --emit` 逐字生成粘贴——引用纪律第六周期；时点 = 定稿链末笔；1:1 回填保持树行数不变，(b) 两口径随之稳定）：**

```
[gatecheck:emit] (b) 全量 = 36 files / 2770 / 64
[gatecheck:emit] (b) 自排除 = 35 files / 2601 / 64 (口径 = -- ':(exclude)v2.27开发交付__*.md :(exclude)versions/v2.27/DELIVERY*.md')
[gatecheck:emit] (d) 全集 32 份 → OK 7 / 未登记实质变更 14 / 自指滞后 3 / 缺声明行 7 / 其他 1
[gatecheck:emit] (a) CHANGELOG [2.27.0] 节日期 = 2026-10-09; 全文日期计数 = 2026-09-29×5 2026-10-01×3 2026-09-25×3 2026-09-24×3 2026-09-12×3 2026-10-02×2 2026-09-30×2 2026-09-27×2 2026-09-09×2 2026-09-06×2 2026-10-09×1 2026-10-07×1 2026-10-06×1 2026-10-05×1 2026-10-04×1 2026-10-03×1 2026-09-26×1 2026-09-19×1 2026-09-11×1 2026-09-08×1 2026-09-07×1 2026-09-05×1
[gatecheck:emit] 登记 token = v2.10开发交付__挂账清偿与板侧闭环.md=22 v2.11开发交付__勘误批与轨道收口.md=3 v2.12开发交付__勘误清偿与判据自洽.md=4 v2.18开发交付__碰撞面根治实施与记法机制化.md=4 v2.19开发交付__勘误更正与时点纪律收口.md=5 v2.20开发交付__定稿后复测机制与基线口径明示.md=38 v2.21开发交付__勘误收敛与封闭枚举.md=33 v2.22开发交付__非代码清欠与复测脚本化.md=29 v2.23开发交付__引用机检闭环与纪律块重组.md=26 v2.24开发交付__勘误收敛与verify时点固化.md=20 v2.9开发交付__模板口径澄清与板侧同步窗口-r2.md=1 v2.9开发交付__模板口径澄清与板侧同步窗口.md=54 versions/v2.25/DELIVERY.md=14 versions/v2.26/DELIVERY-r2.md=1 versions/v2.26/DELIVERY.md=9
```

### 7.5 红文档格局对照（全集口径续行——32 份）

- **基线（时点 = v2.26-r2 定稿，`versions/v2.26/DELIVERY-r2.md` §7.5 + 行⑧ 片段）**：全集 **31** 份 → OK **7** / 未登记实质变更 **14** / 自指滞后 **3** / 缺声明行 **7** / 其他 **0**。
- **v2.27 定稿时点对照（重枚举，不复用上轮值——gatecheck (d) 类实测：全集 32 份 → OK 7 / 未登记实质变更 14 / 自指滞后 3 / 缺声明行 7 / 其他 1（= 本档，记录终点 `v2.27` 在 tag 前不存在，分段形态接受；tag 后转 OK 即 8/14/3/7/0），行⑧ 片段承载）**：全集 **32** 份（31 + 本档）→ 分类值以发版点机读输出为准逐字粘贴于 §7.4⑧（禁预抄）。**发版点复现预期（tag 后）**：本档由定稿时点分类（tag 符号未存在 → 区间右端不可测，属"其他/自指滞后"桶，分段形态接受）转为 **OK**（经典路径 `snap-2.27-start..v2.27` 可达）；既有七链 token 各随本版非白名单点名面增补（C6 收口）。评审按 tag 后 worktree 复现；如偏离逐条解释。碰撞红**零残留跨十一版**（v2.18–v2.27 全集枚举无碰撞形态红——本档散文行不携 token、红原文一律入围栏，`@` 前缀收紧形态在位）。

### 7.6 发布物（tag 与 Release）与终态门批次

- 快照 `snap-2.27-start`（`fb7bbd9`）打后触发面实测（2026-10-08，本地 tag 未推送）：`gh run list --limit 3` 最新三 = CI main `37641961837`（2026-10-07，v2.26-r2 R6b）/ Release v2.26 `37607203383` / CI main `37607201977`——**无新 run**；`gh release list` = v2.26 Latest / v2.25 / v2.24 / v2.23 / v2.22——**无新增**（HC-2 禁"无副作用"先验断言，此为实测）。
- **覆盖率行**（docsync 交付复现表检查的承载）：全量回归四形态（520/520/527/521，双平台 CI）+ ex 五配方 + 机制门四项（docsync / docref / apidump / sizecheck）+ **板上自测 22 套件**（`AT+SELFTEST` 实测 22/22，本版无新增套件）+ 板侧走单五项（§17.2，含 `AT+METRICS`/`AT+FEATURES` 两条新命令真实读数）+ `make WERROR=1` 零警告门 + ASan 双变体（CI 侧 Linux 常设，本机 MinGW 无 libasan 不跑，如实声明）——测试与质量门覆盖面随本版库面新增同步扩大（新增 23 例）。
- P3-5 终态门批次（定稿链末笔实测，留痕 `build/final-gates-v227.log`）：`docsync` = pass=370 fail=0（[自指] 三方一致 + 基线单一来源断言绿 + 特性表↔开关数断言绿 + 本版新增 22 断言全绿）；`apidump --check` exit 0；`apidump --diff` 对默认基线 v2.26 = **新增 28 项 / 修改删除 0 项**（发版点复读，与 AC-3 点名清单一致）；`sizecheck` = pass=3 fail=0；docref 逐档点名面按实测收录后 = OK（本档 tag 后经典路径）；全量回归四形态 = 520/520/527/521 fail=0 + `mingw32-make ex` 五配方 PASS（留痕 `build/ex-v227.log`）；四门零改动点名（`git diff --numstat snap-2.27-start..HEAD -- tools/docref.sh tools/gatecheck.sh tools/sizecheck.sh tools/bench.c` = 空）；gatecheck `--emit`/`--verify` 留痕在案（§7.4 行⑧——verify PASS 后方打 tag）。
- `git tag v2.27` = 终态提交（早于下一版代码合入 `main`）；Release v2.27 = Latest 四附件（demo.exe / SHA256SUMS / stm32f103_demo.bin / stm32f103_demo.elf）；CI 终态 run 七 job success；发版后删除 `dev/v2.27` 并确认 `git branch --list "dev/*"` = 空。

### 7.7 归档（P3-1）

- `docs/API_INVENTORY_v2.26.md` 由 `git worktree add --detach` 于 `tag v2.26`（`074759f`）用当版 `apidump --snapshot` 真生成（**433 项**，`item_lines` 口径 = 433）；**blob 层 sha256 = `694ddfd03d242b6b04dee8c5098e4a8e697de37ad516eb2fc5b5d90c608e4a98`**（blob 层口径，`git hash-object` = 72651529ffcba19775c501f86b71c710b171de46，`git cat-file blob` 复跑逐位一致）。
- 与 v2.25 归档差异 = **仅 `ET_VERSION_MINOR` 25→26 一行**（blob 层 `diff` 实测 2 命中行 = 1 删 1 增；条目数 433 不变；v2.25 归档 blob sha256 `7db21135f0cc9a9a29b7dcb7695038f9697f91d95b274e8f66f393897f74e9a5` 复核一致）——公开面零变化，预期命中。
- 生成环境注记：Git Bash（Windows），`core.autocrlf=true`；gen_body 产出 LF；禁手工编辑（全程机读生成）；一次性 worktree 用后即删（`git worktree list` 终态仅主树）。
- `tools/apidump.sh` BASE 切 `docs/API_INVENTORY_v2.26.md`（`:15` 头注 + `:227` 默认值 + 滚动履历续写）；旧归档零触碰（`git diff snap-2.27-start..HEAD -- docs/API_INVENTORY_v2.25.md` = 空）；README 基线名前滚两处与 docsync 基线单一来源断言同批（偏差 5 的合批根因）。

## 8. CO 处置（三落点——计划 §1.1 ↔ 本节；v3-candidates「v2.27 复评记录」节随本档 C5 建立）

| CO | 项 | v2.27 处置 | 触发条件 / 备注 |
|---|---|---|---|
| **CO-6(v2.26)** | r2 交付 verify「双点同值」声称只载一点 | **已修**（P0-1，C0 = `ba09e66`）：措辞改 `--emit` @R6a + `--verify` @R6b 单点绿，四处同批（§1/§2/§7.4⑧/§7.6）+ 引文压缩形态加注记；本交付 §7.4 行⑧ 遵新口径书写 | AC-1 |
| **CO-7(v2.26)** | 四处机械声称与实测不齐（numstat/枚举/fail 计数/时点差面） | **已修**（P0-2，C0）：五处按值勘误（26+/0− / 14 全列 / fail=1 双诊断行 / (b)(d)token 三面）+ 机制建议入两模板与 `templates/DELIVERY.md`（P0-3） | AC-1/AC-2 |
| **CO-8(v2.26)** | rN 档存在时 [自指]/docref 无参定位切换盲区 | **条文化 + 入池**（P0-4，C0）：两模板各一行显式化"对象 = `head -1` 命中的 rN 档、主档声明不再被机检、无参 docref 恒红属预期态"；三条门修法（遍历全档 / docref 回退或并列 / gatecheck `--from <sha>`）入 `docs/v3-candidates.md`「来自 v2.27 候选池登记」节 | 本版零工具改动；触发 = 首个带 rN 轮的功能版或 M2 开窗 |
| **CO-9(v2.26)** | 门新增超出冻结计划允许面且授权链未落纸 | **已修**（P0-3，C0）：条文入**权威载体**（开发计划模板 §2 门改动授权通道 + `templates/DELIVERY.md` 偏差表固定列）；**本版即首个实例**——P1 的 docsync K=22 断言块全在 HC-7 授权面内，同批 architecture 回填 370 + N 复算（§7.1）+ docref 登记（C6）；**偏差表按新列形态落地 = §3 表第五列「计划面外授权」在位、8 行各 = 无**（**[v2.27-r2 T-3 更正，`CO-3(v2.27)`]** 原式自称「按新列形态登记（§3 偏差 4/5/6）」时该列尚未落地 = 自述失准，r2 已按含该列形态重排并补 T-4 偏差 8） | AC-2 |
| **CO-1…CO-5(v2.26)** | r1 评审五条（占位符 / 锚点 / 计数 / 三落点 / §4 SHA） | **已关闭**（v2.26-r2 清偿）；本版仅复核不重做——复核在案：docsync 占位符拦截两行绿、终点锚形态遵新条、按值声明沿用、三落点链无缺位、§4 SHA 一律实测 | 计划 §1.1 行 8–12 |
| **M-3 / M1 到期** | M1 ②③④ 触发未现、窗口到期 | **已落**（P0-5，C0 + 入仓批）：ROADMAP 新建 M3（判据四条 = 本版交付项一对一）+ M1 结案（① 达成；②③④ 移出阶段判据转常设挂账，逐条点名承接载体） | §1.0 ④ 路；BACKLOG REQ-2/3/4 触发不变 |
| **REQ-6 / REQ-8 / REQ-9** | et_metrics / AT+FEATURES / sched 失准计数 | **已交付**（P1-1/P1-3/P1-2）：M3-①②③ 判据各有一条板上读数对应（`AT+METRICS`、`AT+FEATURES`、`loop_miss=2`） | AC-3/AC-4/AC-5/AC-6 |
| **REQ-5 / REQ-7** | 板侧窗口自动化 / 真固件 IAP 常设化 | **本版不认领（维持已接受）** | 理由 = 计划 §1.1 行 16；本版窗口留下手工基线与逐项读数形态作脚本化对照真值；REQ-7 随 REQ-5 联动 |
| **REQ-1 / CO-6(v2.8)** | bench 相对比值化 | **已关闭**（v2.25 落地，r1/r2 两轮评审独立复现）；REQ-1 状态位经 Owner 2026-10-08 授权代改转「已完成」（BACKLOG 归因注记在案） | M1-① 达成 |
| **CO-4(v2.8)-M / CO-9(v2.8)-M / CO-7(v2.7)-M** | docsync 内嵌 `--check` 漂移 / apidump 阈值 / 分支 A | **暂缓（维持现状）** | 触发逐条：漂移复现即计；本版公开面**纯追加 28 项**（阈值重校准触发必不现）；分支 A = v3 破坏性窗口（M2） |
| **CO-8(v2.8)-P / ch32x035 / f103** | USART2 双口 / ch32x035 回归债 / f103 真机 | **暂缓（挂账续期）** | 本版窗口 = 仅 G474（COM16/WCH-Link 实测不在位——§17.4）；常设条目 v2.27 行在档 |
| **PF-1** | G474 逐版状态无常设承载 | **已落**（P2-4，C3）：`docs/v3-candidates.md`「G474 板侧同步窗口义务」结构化条目（触发 = 每版改库面即窗口义务；Owner 可整条删除，删除后果在档） | AC-6；§6 首条 |
| **池内条目** | 载体形态 / c-2 白名单化 / "扩展 docref"变体 / doc↔log 常设化 / 三条 rN 门修法 | **暂缓维持 v3 池** | 本版零工具改动（除 HC-7 授权面的 docsync 断言块与 apidump BASE 数值） |

**板侧面声明**：本版**有 G474 板侧窗口并已真实执行**（P2-1…P2-4，实机记录 §17）——同步/构建/烧前机检/烧录/走单五项/证据八件落盘全链在案，跨 16 版同步债清偿；ch32x035 挂账续期（触发未现，工程本体零触碰）；f103 真机缺板维持挂账（编译 + Renode 仿真门常态兜底）。

**v2.27 新增破坏性候选：库面零破坏（MINOR 只追加）。** 公开 API `apidump --diff` = 新增 28 项 / 修改删除 0 项（AC-3/AC-7）；`et_task_t` 结构体字段追加（`sizeof` 16B→24B，破坏半径评估 = `docs/API_STABILITY.md` §5.1 v2.27 行）；六既有签名逐字不变（AC-4）；门判定语义零变更——`tools/docsync.sh` 纯新增断言块（只扩检查面，含一条反向依赖断言与一条数对账断言），`tools/{docref,gatecheck,sizecheck}.sh` 与 `tools/bench.c` 零 diff，`tools/apidump.sh` 仅 BASE 数值两处（`git diff --numstat` 实测 2+/2−，[v2.27-r2 T-1 勘误，`CO-1(v2.27)`]）；库外 = 板侧私有工程同步与 demo 改动（不在本仓 diff 面）。
