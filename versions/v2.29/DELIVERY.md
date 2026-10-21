# v2.29 DELIVERY：首次接入面（CMake）、并发契约表与真 A/B 分区（功能版）

> 本版 docsync：**382/382**（终态实测，行首声明供 [自指] 断言对账；推导逐步式见 §7.1，与 `docs/architecture.md` 机制门行同值）
> 交付日期：2026-10-20（终态提交日）｜ 执行者交付，输入 = `versions/v2.29/PLAN.md`（已批准-冻结，出口 A 功能版·全量档）+ `versions/v2.28/REVIEW.md`（r1，有条件通过，`CO-1…CO-12`）+ `versions/v2.28/DELIVERY-r2.md` §5（r2 回收 `RC-1…RC-5`）
> **显式声明缺失**：`versions/v2.28/REVIEW-r2.md` 不存在 ⇒ r2 期新发现由计划 §1.1 人工回收为 `RC-1…RC-5`，处置与本档 §8 同权重（沿 v2.28 对 r1 缺失评审的同一手法）。
> 首行声明（末笔身份单点承载，`RC-5` 新条文首例）：**末笔 = 本档 §4 记录范围首行所指围栏首行的那一笔**，其余各处一律写「见 §4 首行」，不在他处重复声明末笔身份。
> 开工前置（HC-0）五项实测值 = AC-0 行承载；`versions/v2.29/REVIEW.md` **尚不存在**——阶段 4 由审核方（≠执行者）独立会话产出，本档为其输入；**本版评审记录（`versions/v2.29/REVIEW.md`）在落笔时点缺失属预期时序，此处按门要求显式声明其缺失**（模板 §1 与 `docsync` [评审记录] 断言要求交付档「引用评审记录或声明其缺失」）。

## 1. 交付摘要

主题 = **让第一次用本库的人能把库装进自己的工程**（M4-①②③④）：

- **P0 采纳前置与条文批（G0 窗口）**：MIT `LICENSE`（版权行 = `git config user.name` 实测 `azura`）；README **支持矩阵** 17 数据行 × 三档封闭取值（**验证过 10 行**逐行带命令/CI job 名/节号，**未验证 4 行**，**不支持 3 行**；三档分布按状态列逐行统计 = 10/4/3——`grep -c` 会把表头说明行计入）；`examples/ex_min.c` 十分钟最小样机（五源文件最小闭环）+ `getting-started` §0.5 一页纸（命令逐条从文档提取真跑，含 `ET_MODULE_CRC=0` 关↔开符号读数 **0 ↔ 8**）；两模板**五处条文**（§7④-bis 体量/容量维必填、双时点标注、申报件白名单归属、末笔身份单点承载 + 1:1 补充笔上限、DELIVERY §8「新增破坏性候选」落点义务）；ROADMAP 新建 **M4** 四判据 + **M3 结案归档**。
- **P1 N-01 CMake 接入面**：根 `CMakeLists.txt` + `cmake/etConfig.cmake.in` + 两枚交叉 toolchain 文件；目标 `et::embedded_tools`；**版本与 32 枚模块开关默认值全部从 `et_config.h` 正则读出**（零复制字面量）；`install/export` + `find_package(et CONFIG)`；消费侧两形态**各 5 行**实测可编可跑；`docsync` 双源一致性断言（CMake glob ↔ Makefile `*_SRC`，七目录 35 文件一条）+ CI 新增 `cmake integration` job（不替代既有 Makefile job）。
- **P2 REQ-7 真 A/B 分区（契约面）**：`port/port.h` 纯追加区域 API 五符号 + `port_flash_region_info_t` + 能力宏 `PORT_FLASH_REGIONS`（三既有签名逐字不变）；host/G474 真实现、**F103 显式不支持**；`et_bootctl_cfg_t` 尾追加 `slot_base[2]`（20→28 B，双目标实测）；host 新用例 6 例（跨区域 stage/回滚、超槽、超区域、不在分区表、混合位形、边界与掉电）；**板侧真分区落地并实测**（`AT+REGIONS` 四条 + 体量 87,872 B ≤ 槽 131,072 B + 链接脚本 512K→128K 护栏）。
- **P3 并发契约表与工具面**：`docs/concurrency.md` 35 行 × 4 列，逐行带 `文件:行` 头注原文（64 条引文机核全命中），取值含**未声明**与**冲突**两档并登记三处真实缺口；`board_window.py` 串口异常面（有界重试 + 证据件必生成 + 退出码分类 2/1）；内存门一条命令本机可复现（`ASAN_DLL_DIR`）+ 三处「本机不可执行」按值勘误。
- **P4**：基线滚动三件事（`API_INVENTORY_v2.28.md` 由 worktree 于 tag 真生成 468 项 + BASE 两处切 v2.28 + 发版点复读 **7 项新增 / 0 删改**）、数字回刷同批（版本宏 29、用例数 **520→526**、例数 **5→6**、两 port v2.29 体积行、CHANGELOG `[2.29.0]`）、登记链收口（九档各 +1 纯登记行）、本档。
- **未完成 = 一条**：REQ-7 **真固件双单的传输面**（88 KB xmodem 在 block ≈ 91 由接收端 CAN 中止，两次独立复跑同点）⇒ 申报 `BLOCKED-2`，AC-8 该子条按未闭合登记，**库面零绕行、未以小合成镜像冒充真固件**。

## 2. AC 自查表

| AC | 结论 | 实测读数与证据命令 | 判据出处 |
|---|---|---|---|
| **AC-0** | **通过** | HC-0 五项：① 真板走单复跑 `python tools/board_window.py --mode full` → 本版窗口内再跑 **rc=0**（五要素 + 板侧七项读数在 `G474VET6_ET_TEST\build\v2.28-evidence-rerun\` 与本版 `v2.29-evidence\`）；② `CO-8(v2.28)-P` 裁决 = **路径①'**（Owner 2026-10-20，变更记录在档）⇒ 本版落 P3-3；③ `git merge-base --is-ancestor ef339dd main` **exit 0**；④ `git tag --list v2.28` 非空、`git rev-parse --short 'v2.28^{}'` = **`ef339dd`**（**不是 `8a1f060`**，r2 勘误后口径）；⑤ CI run `39359483532` success、Release run `39359485783` success、Release **v2.28 = Latest** 四附件、`git branch --list "dev/*"` 于本版开工前 = 空 | HC-0 / §6 ① |
| **AC-1** | **通过** | `LICENSE` 在位 1,062 B；`MIT License` 行 = 1；`Copyright (c) 2026 azura` = 1；README「**许可证**：MIT」链接行 = 1（`build/p0_criteria.log` 行 1-2） | P0-1 |
| **AC-2** | **通过** | 支持矩阵节在位 = 1、数据行 **17 ≥ 12**；「验证过」**10** 行**逐行带反引号命令或 CI job 名**（机检 = **10/10**；三档分布 = **10 / 4 / 3**，按状态列逐行统计——`grep -c` 会把说明行计入而给 11）；`docsync` 断言在位（实测 `ok README 支持矩阵节在位`）；抽检两行独立复跑：`make WERROR=1 test` rc=0、`python tools/board_window.py --mode full` rc=0 | P0-2 / HC-8② |
| **AC-3** | **通过** | `examples/ex_min.c` 在位（**6,076 B**，`wc -c` 口径；落仓笔 `20e767a` 的 blob 同值 ⇒ 非后续漂移）；`make ex` rc=0 且六配方各出 PASS；CMake 路径 `-DET_BUILD_EXAMPLES=ON` + `ctest` **6/6 Passed**；单条 gcc 五源命令 rc=0 输出与 §0.5 预期块**逐字同**（探针 = `build/verify_p0_3.py` 从文档表格提取反引号段执行）；例数面 = 实测 `ls examples/ex_*.c` **6** ↔ README「扩至六例」↔ architecture「make ex: 六例」三处同值（docsync 断言绿） | P0-3 |
| **AC-4** | **通过** | 五处条文各命中 = 1：`④-bis 体量/容量维` 1、`取数时点 SHA` 计划 1 / 评审 1、`白名单归属须显式` 计划 1 / 评审 1、`末笔身份单点承载` 1、新句 `段为落点义务` 1（模板内 `新增破坏性候选` 二字面 = 2，另一处 = v2.26-r2 G0-T2 历史反例引用，非本笔新增）；纯追加性 = 计划模板 4+/1−（1 为 §7④ 严格前缀扩展）+ 评审模板 2+/0−，真改写 0（程序比对旧行为新行前缀）；本档 §8「新增破坏性候选」段在位 | P0-4/P0-5 / RC-4 / RC-5 |
| **AC-5** | **通过** | `^\*\*M4` = 1；M4 四判据行 = 4（逐条指到 P0-1~P0-3 / P1-1~P1-3 / P3-1 / P2-1~P2-4）；M3 收尾状态行在位且 ⑤ = 达成（与 v2.28 HC-0① 实测一致）；M3 段位于 `## 已结案阶段（归档）` 内（行 27 < 38 < 51 实测） | P0-6 |
| **AC-6** | **通过** | ① 两形态接入工程**各 5 行**（`add_subdirectory` / `find_package`）可编可跑，输出 `[ac6] ringbuf+kv OK port=host ver=…`，rc=0；② ARM 交叉 `arm-none-eabi-g474` / `f103` 两 toolchain 构建 **0 warning**（`grep -ci warning` = 0）；③ `-DET_MODULE_KV=0` 后 `nm` **无 KV 符号**：host obj `et_` 定义符号 0（对照默认位形 10）、ARM 归档 `--defined-only et_kv` 10→**0**、`et_kv.c.obj` 文本符号 27→0；④ `cmake --install` 后另一工程 `find_package(et CONFIG)` 成功；⑤ **MINOR 升级位形接入码零修改 = 本版内两次配置**：同一份未改动消费文件在 `ver=2.28.0`（P1 批）与 `ver=2.29.0`（P4-2(a) 批）各跑一次，两次 rc=0 且版本号随库变（`build/p1_ac6_evidence.log`）；HC-8① 口径 = 无目标代码与符号，全 `.c` 一律参与编译 | P1-1 / REQ-10 |
| **AC-7** | **通过** | docsync 双源断言在位（`ok … 七目录 35 文件`）；**反向探针两点**（临时树 `git archive HEAD` + 叠加未提交门面，主树零写入 = `git status --porcelain Makefile` 空串）：删行内项 `core/et_ringbuf.c` → `pass=378 fail=2` 点名 FAIL；删行尾项 `debug/et_metrics.c` → `pass=377 fail=3` 点名 FAIL（同时触发 v2.27 既有 `Makefile 含 metrics 源` 断言 = 双门同红，如实登记非我一条把守）；CI 新 job 存在且不替代既有 job（`yaml.safe_load` jobs = unit-tests / coverage / cmake-integration / cross-stm32f103 / cross-stm32g474 / docbuild / renode-smoke），`ci.yml` diff 仅新增 job 段（85+/0−） | P1-2 |
| **AC-8** | **部分达成 = 五条达成 + 双单未闭合（BLOCKED-2）** | 达成：① 分区表**实测**（`AT+REGIONS` → param 0x08078000/32768/2048、slot_a 0x08000000/131072、slot_b 0x08020000/131072、state 0x08040000/2048、`regions=4`，来源 = 板侧 port 分区表本体 + 链接脚本 + CFG）；② 体量 ≤ 槽容量**算式 + 实测双读数**（真固件 body 87,840 B → 打包 87,872 B ≤ 131,072 B；链接脚本 FLASH 512K→128K = 超容在链接期失败）；③ host 单测（跨区域 stage/rollback、超槽、超区域容量、基址不在表、混合位形、边界/页擦/掉电）**6 例全绿**，套件 24→30；④ `apidump --diff` 新增 7 项 / 修改删除 0 项（对 v2.27 与 v2.28 两基线交叉一致）+ `API_STABILITY` §5.1 追加行 = 1；⑤ f103「不支持区域 API」声明在位（`PORT_FLASH_REGIONS 0u` + 五符号恒失败实现，两板构建零警告）。**未达成**：真固件双单（confirm 的 `AT+BOOTINFO` 三字段 + `AT+VER` / 回滚的 A 槽存活 + `attempts` 递增）——88 KB 传输在 block ≈ 91 由接收端 CAN 中止，两次独立复跑同点；且本窗口 `AT+BOOTINFO` 的 `staged=1 confirmed=1 attempts=1` **系参数区残留态**（烧录不擦状态扇区），不作 confirm 证据 | P2-4 / HC-10 |
| **AC-9** | **通过** | `docs/concurrency.md` 表行 = **35**，`docsync` 断言「与模块集合**逐名**一致」绿；引文机核 = `build/p31_verify_cites.py` 解析每行 `:NN`+「引文」回读该文件对应行 → **64 条引文 / 未命中 0**（含 ≥5 行抽检要求的覆盖面）；无「FreeRTOS 已验证」表述（矩阵行 = 未验证 + 表顶声明「FreeRTOS port 未验证」） | P3-1 |
| **AC-10** | **通过（"释放后可开"一例除外，如实登记）** | 占用注入实测（`build/p32_evidence.log`）：用例 2 子进程真占 COM12 ⇒ 有界 6×0.8s 返回 **None、未抛异常、4.80s**；用例 1 不存在端口同理 None（FileNotFoundError 亦捕获）；证据件面：`--mode full` 中止时 `window_run.log` 与已生成件在档（本轮 5 件），退出码 **2 = 端口不可用 / 1 = 机检失败** 与 `--help` docstring 一致；正常路径不变 = `--mode check-only` **rc=0**；**未闭合子例** = 「释放端口后可开」——释放等待累计 ~13 s 仍 `PermissionError(13)`，`Win32_Process` 归因 = Owner 侧应用 PID 43224（非本仓探针残留，Agent 未杀该进程）⇒ 该例待端口归还复跑，本档不预写其绿 | P3-2 / RC-1 |
| **AC-11** | **通过** | `docs/API_INVENTORY_v2.28.md` 在位：worktree 于 `tag v2.28` = `ef339dd` 真生成（`git worktree add --detach` → 该树内 `sh tools/apidump.sh` → 回贴 → `worktree remove` 后仅剩主树）；**468 项**、24,798 B、sha256 `12d4747…884e4e72`；与 v2.27 归档 diff = **2 行**（`ET_VERSION_MINOR 27`↔`28`）⇒ 计划「v2.28 公开面零新增 ⇒ 预期仅版本宏行」由实测证实；`apidump.sh` BASE **两处**实测切 v2.28（`:227` 默认值 + `:15` 头注示例）；发版点复读形态正确（新增 7 / 删改 0，非 0/0/0） | P4-1 |
| **AC-12** | **通过** | MINOR **29** / `0x021D00`：`gcc -E -P -dM et_config.h` = MAJOR 2 / MINOR 29 / PATCH 0，探针 program 实测 `ver=2.29.0 hex=0x021D00`；README 版本行 + 交付链接行 + 基线名前滚 ×2（→ v2.28）；例数 **6 五处**同值；用例数回刷面 `grep -rn` 终态全列点名（README×2 / architecture / API_GUIDE×2 = 526 同值 + getting-started×2 + 两 port host 行 + docsync 两枚字面）；模块数 **35** / 开关数 **32**（`grep -c '^#define ET_MODULE_'` = 32，并发表行数 = 35 两口径并存且断言分别将守）；CHANGELOG `[2.29.0]` 日期 = 2026-10-20 = 终态提交日；两 port 体积行构建实测（f103 25760/32/1520 与 38204/32/3808、g474 26432/36/1520）⇒ `sizecheck pass=3 fail=0` | P4-2 |
| **AC-13** | **通过（内存门按 HC-0② 裁决登记）** | 四形态实测 **526/526/527/533**（`make WERROR=1 test` / `test-g4` / `test-1k` / `test-tab` 各 rc=0，基线 520/520/527/521 各 +6）；`make ex` 六配方 PASS rc=0；`docsync` = N/N fail=0（见行首声明与 §7.1）；`apidump --check` rc=0；`sizecheck` 3/0；`docref` 逐档点名见 §7.3；CI 七 job（含新 `cmake integration`）与 Release 四附件 = 发版步面，本档 §6 登记其执行主体；快照触发面实测见 §7.6；**留痕件 = `build/final-gates-v229.log`**（库外、零入库，六段 = 声称面，含 `--verify` stdout 与退出码）| P4-2b / REQ-18 |
| **AC-14** | **见 §7.4⑧ 与 §7.5** | ⑧ 五行 = `--emit` 逐字粘贴（`diff` 空）；`--verify` PASS @定稿链末笔；(d) 全集份数与 OK/未登记/自指滞后/缺声明行/其他 逐值取自 emit（非本档自述）；token 机读与 §7.3 清单一致 | P4-4 / P4-5 |
| **AC-15** | **通过（含一项偏差）** | G0 窗口谓词逐文件实测（`build/g0_predicate.log`）：六库目录 + `et_config.h` + `docref.sh`/`gatecheck.sh`/`sizecheck.sh`/`apidump.sh` + `.github/` diff **全空**；`docsync.sh` 计划内排除并归因（本版 K=3，分属 P0-2/P1-2/P3-1）；触碰面逐件非空并归因（LICENSE / ex_min / README / getting-started / architecture / 两模板 / docsync / ROADMAP）；**偏差 1**：`Makefile` **全量 20+/4−**（P0 `20e767a` 8+/1− + P3-3 `e752234` 12+/3−）与「Makefile 零 diff」冲突，见 §3 行 1 | P0 G0 / HC-8 |

## 3. 偏差记录

| # | 偏差 | 计划面 | 实测面与归因 | 授权/放行依据 | 后果与去向 |
|---|---|---|---|---|---|
| 1 | **G0 窗口内 `Makefile` 被触碰全量 20+/4−（两笔）** | HC-1 允许面无 `Makefile`；AC-15/G0 窗口判据写「`Makefile` diff = 空」；P0-3 判据与 HC-5 例数回刷面均**未列** `Makefile`；**P3-3 `e752234` 同形态**：其判据原文点名「由 `Makefile` 变量或拷贝运行时 DLL 承担」而 HC-1 允许面同样无 `Makefile` ⇒ 属「判据提到但允许面漏列」的新形态（评审 CO-1） | **全量 = `git diff --numstat snap-2.29-start..HEAD -- Makefile` = 20+/4−**，两笔构成：P0 `20e767a` = **8+/1−**（`EX6_SRC` 一行 + 第六 exe 规则 + `ex:` 目标行，零构建语义变化）；P3-3 `e752234` = **12+/3−**（`ASAN_DLL_DIR ?=` 变量 + 两处 `cp -f "$(ASAN_DLL_DIR)"/*.dll` 配方 + 注释勘误，内存门可复现面所需）。成因 = **计划内在互斥**：HC-5 要求 README:218「`make ex` 一键跑六个」与 `docs/architecture.md`「make ex: 六例」同批改 6，而 `Makefile` 的 `ex` 配方是**逐文件点名**（无 wildcard），不接线则该声称在本批落地时为 false | 取「不留 false 声称」侧（HC-8①③ 为**不可偏离**项，AC-15 为单项判据）；同族先例 = v2.28 偏差登记与 CO-9(v2.26) 授权通道 | 已在 P0 提交消息与本档双重登记；**建议**（供 v2.30 计划）：把「例数回刷含 `make ex` 声称时，`Makefile` ex 配方属计划须列的允许面」写入模板 HC-1 例数条目，消除此类互斥 |
| 2 | **结构体尾字段使四处位置化初始化器补 `{ 0u, 0u }`**（`test/test_bootctl.c`、`test/test_selftest.c`、`examples/stm32{f103,g474}_demo.c`） | HC-1 列 `test/test_bootctl.c`；**未列** `test/test_selftest.c` 与 `examples/stm32*_demo.c` | `-Werror=missing-field-initializers` 在两测试件、`-Wextra` 在两板 demo 强制报警（ARM 零警告门 = CI 常设），不补则四门全红 | 尾字段追加的**必要伴随面**（零语义变化：0 = 旧位形，实测既有 24 例逐值仍绿、改后 bootctl 30 例含新 6 例全绿） | 登记于 `API_STABILITY` §5.1 v2.29 行「可见代价」段；建议模板 §5.1 追加行字段增列"编译面连带改动"一项 |
| 3 | **能力宏由「编译期裁剪」改「运行期显式拒绝 + 恒失败实现」** | P2-1 与 HC-9 写「未实现区域 API 的 port 以编译期宏声明 ⇒ bootctl 拒绝该路径」 | 宏只有平台自己的头看得见，而 `storage/et_bootctl.c` 只包含 `port/port.h` ⇒ 编译期 `#if` 摘掉调用会让未实现平台变成**链接错误**。改形态后语义不放宽：f103 仍显式声明 `PORT_FLASH_REGIONS 0u`，bootctl 仍在 lookup 假返回时**拒绝**（host 用例 `bc.region_unsupported` 覆盖该路径） | 判据「不静默降级」保真，形态属计划可裁量项①（区域 API 具体形态） | 已在 P2a 提交消息与本档 §2 AC-8 行登记；`port.h` 注释写明理由防回退 |
| 4 | **板侧 P2-4 未完成（非"计划面外改动"，而是计划任务未达成）** | P2-4（AC-8 双单） | 见 §2 AC-8 行「未达成」段与 `BLOCKED-2__v2.29真固件双单传输面受阻__88KB_xmodem在约11KB处失步.md` | HC-10⑤：板侧红 = 真实发现走 BLOCKED，不改库面绕过 | AC-8 该子条判未闭合；M4-④ 判据本版不达成（转下版）；两条候选入池（`docs/v3-candidates.md`「来自 v2.29 候选池登记」A/B） |
| 5 | **P3-2 提前于 §6 顺序 ⑧（本应在 P2-4 之后）** | §6 执行顺序 ⑤→⑥→⑦→⑧ | 板侧窗口跑到 ⑤ 烧录后自口时**现场复现 `RC-1`**（`PermissionError(13)` traceback，`window_run.log` 与走单件全丢），而 P2-4 的双单被同一缺陷挡住 | 修障在前才能完成 P2-4；`board_window.py` 属 HC-1 允许面（RC-1 专项） | 顺序偏离在此登记；P3-2 提交消息同步登记；未改任何库面 |
| 6 | **库外工程侧三处修复超出 P2-3 字面清单** | P2-3 写「分区配置 / 链接脚本与私有 demo 配置同批」 | 实测发现并修：① 工程内 `Core/et/port.h`（2026-10-08）陈旧副本在 `-I` 顺序上遮蔽同步件（构建 exit=1 而**同步门报残差 0**）；② `cmd_upgrade` 接收路径内逐页擦 ⇒ 改整槽预擦；③ `boot_flow()` 自检按旧参数区地址取镜像头 ⇒ 改按 `slot_base` 经区域 API 取（详见实机记录 §19.4） | 均在库外允许面（`G474VET6_ET_TEST\**`），不改库面语义 | 陈旧件保留为证据 `build/v2.29-evidence/port.h.stale-20261008.shadow.txt`；①属"第二源遮蔽"同族问题，已入 v3 候选 B 的归因段 |
| 7 | **六笔提交消息的「合计 numstat」按值声称失准**（本档实测暴露） | 模板 §5「按值声称」纪律与 `CO-7(v2.26)` 族（v2.28 亦有一例 R12） | 复算命令 = 对 `snap-2.29-start..4b991fc` 逐笔 `git show --numstat --format=""` 交 `awk` 求和，与消息内声称值比对。**六笔失准，方向与面各不相同**：`20e767a` 声称 306+/22− / 实测 **286+/21−**（**逐件值全部与 numstat 同值**，仅合计多算 20+/1−）；`057cd2e` 声称 362+/4− / 实测 **422+/3−**（逐件 8 项同值，合计漏计 `ci.yml` 85+ 一侧）；`3307a1b` 声称 690+/63− / 实测 **690+/59−**（插入侧**逐值同**，删除侧多 4）；`cc6d1b7` 声称 8+/7− / 实测 **8+/5−**（同上，删除侧多 2）；`d701c41` 声称 `board_window.py` 单件 48+/9− / 实测 **73+/21−**（**唯一一件逐件值也失准** —— 该笔只列单文件，无合计可推）；`8718397` 声称 17+/27− / 实测 **37+/27−**（逐件 21+/15+/1+ 合计 37，删除侧同值）。余五笔（`68dd7dc`/`4f1487a`/`e752234`/`2b54f62`/`4b991fc`）声称 = 实测**逐值同** | **无授权改历史**：提交消息不可回写（不 amend、不强推），故取「本档登记 + 实测值覆盖声称值」的放行路径；失准**不涉及**任何判定面（门读数、符号数、体量、用例数、板侧读数在消息内均为命令实测输出） | 根因 = 合计值由**手算**得出而未回读 `numstat | awk` 求和输出，即 `CO-7(v2.26)`/v2.28 R12「凭记忆成文」同族的**第三形态**（逐件对、合计错）；修法建议入 v3 池 = 模板 §5 增条「提交消息的**合计类**数字须由求和命令输出粘贴，禁手算」，本档不代改模板（P4 无该授权面） |
| 8 | **`build/p44_emit.log` = 混合时点产物（取数期间并发落了一笔提交）** | ⑧ 条文要求「取值 = 本轮机读、引用 = 逐字粘贴」；`gatecheck.sh` 的 (b) 在 `:86` 先算、(d) 循环 `:92-104` 逐档跑 `docref`（数十秒）在后至 `:133` 才打印区间右端 | 该件的 (b) 全量 = **39 files / 2531 / 115** 而首行自述区间右端 = **`4b991fc`**；两点实测复现成因 = `git diff --shortstat snap-2.29-start..2b54f62` → **39/2531/115**（与件内值逐字节同）而 `..4b991fc` → **49/2570/115** ⇒ 该次 emit 的 (b) 求值发生在 P4-3 落仓**之前**、(d)/token 求值与首行打印发生在**之后**（我以背景任务跑 emit，与提交并发）。同族的 (d) 桶亦可能被逐档分裂（前半按 `2b54f62`、后半按 `4b991fc`）——**故本档不引用该件的任何值**，⑧ 五锚行一律改取本次**前台单跑**的 emit 输出（无并发写入窗口） | 发现于本批 §4 声明行取数时点（`docref` 对账即报「声明与实测不符」而暴露，非我自查规则命中）；背景任务与提交并发是本仓既有纪律（v2.28 R12「后台通知的退出码属包装层」）已点名的风险面，本笔为其**第二类实例**（值面而非退出码面） | 后果：P4-3 提交消息引用了该件的 (d) 值作「本笔前」判定，其**逐档时点不可复核** ⇒ 该声称以本档 §7.4⑧ 的前台单跑值为唯一权威（原消息不可回写，见偏差 7 的同一纪律）；纪律面：此后 emit/verify 类取值一律前台单跑且跑后再复跑一次对账（两跑 stdout 逐字节同才算稳） |

## 4. 提交清单（记录范围 = `snap-2.29-start..END`）

`git diff --stat snap-2.29-start..4b991fc` = **49 files changed, 2570 insertions(+), 115 deletions(-)**（**END 冻结 = 主批末笔 `4b991fc` = P4-2b/P4-3**，非 `HEAD` 浮动 ⇒ 本档与其后的定稿链笔不落进本区间，沿 v2.27/v2.28 冻结终点口径；三数 = 本笔实测 `git diff --shortstat snap-2.29-start..4b991fc` 原文（**非** `build/p44_emit.log` 的 39/2531/115，该件为混合时点产物，见 §3 偏差 8）。表 1~11 笔逐笔可 `git log --reverse --format=%h snap-2.29-start..4b991fc` 复现，与行首具名 SHA 集差 = ∅）

围栏首行 = **末笔身份唯一载体**（`RC-5` 条文首例）：本档 §4 记录范围的 END 即末笔所指，其余各处写「见 §4 首行」。

| # | SHA | 批次 | 内容摘要 |
|---|---|---|---|
| 1 | `20e767a` | P0 | LICENSE / 支持矩阵 17 行 / `ex_min.c` + §0.5 / 五处条文 / M4+M3 归档 / docsync +1（K=1）+ architecture 机制门行同批 |
| 2 | `057cd2e` | P1 | `CMakeLists.txt` + `cmake/` 三件 + docsync 双源断言（K=2）+ CI `cmake integration` job + §0.9 与矩阵 CMake 行改判 |
| 3 | `3307a1b` | P2-1/P2-2 | `port.h` 区域契约（纯追加 43+/0−）+ host/g474/f103 实现 + `et_bootctl` 位形 + 区域 6 用例（bootctl 24→30）+ §5.1 追加行 + 用例数 526 回刷 |
| 4 | `cc6d1b7` | P4-2(a) | 版本宏 29 / `0x021D00` + 版本键控面（README 版本行、API_GUIDE 适用版本、API_INVENTORY 重生成）+ 两 port v2.29 体积行 ⇒ `sizecheck` 转绿 |
| 5 | `d701c41` | P3-2 | `board_window.py` 有界重试 + 异常走 `abort_reason` + `window_run.log` 必生成 + 退出码 2/1 分类（AC-10） |
| 6 | `8718397` | P2-3a | 分区几何宏归位平台头 + 默认位形修正（A 槽取 0x08000000，附硬件复位向量表理由）+ README 归因行 |
| 7 | `68dd7dc` | P3-1 | `docs/concurrency.md` 35 行 + 逐名对账断言（K=3）+ README/§7 入口；引文 64 条机核全命中 |
| 8 | `4f1487a` | P2-3b/P2-4 | 实机记录 §19 + `BLOCKED-2`（真固件双单传输面受阻，逐值读数与归因限定） |
| 9 | `e752234` | P3-3 | `ASAN_DLL_DIR` 一条命令内存门 + 三处「本机不可执行」按值勘误 + 15 条警告按值复测（10+4+1） |
| 10 | `2b54f62` | P4-1 | `API_INVENTORY_v2.28.md` worktree 真生成 468 项 + BASE 两处 + README 基线名两行 + 发版点复读 7/0 |
| 11 | `4b991fc` | P4-2b/P4-3 | CHANGELOG `[2.29.0]` 30/0 + 九档各 +1 纯登记行（token 全列 10 笔，`(d)` 桶差集实测选定） |

**自指面**：本档（`versions/v2.29/DELIVERY.md`）与后续定稿链笔不在上表内 —— 自指不可预写，由 §4 的 END 冻结终点 + docref「END..HEAD 逐笔分类」覆盖（交付文档属良性白名单面）。**§4 声明行与 ⑧ 的取数时点 = 本档落笔时点，非本笔提交后**（`CO-10` 双时点标注条文首例执行）。

## 5. 机制自证与门改动面

- `docsync` 本版 **+3 断言**（全部纯新增，判定语义零变更 HC-7）：① 支持矩阵节存在性（删标题即红，反向探针手法）；② Makefile↔CMake 源列表双源一致性（反向探针两点在档：删行内项与删行尾项各红一次）；③ 并发表与模块集合**逐名**对账（反向探针两点：删整行 ⇒ `期望=34`；改名 ⇒ `表缺=[ core/et_smap] 表多/名错=[ core/et_smapX]`——**首版写成"数行数"时改名不失配，由自跑探针暴露后改强**）。数值字面改动：`520`→`526` 两枚（HC-7③）。
- `apidump.sh`：仅 BASE 两处（`:227` / `:15`），判定逻辑零改。`docref.sh` / `gatecheck.sh` / `sizecheck.sh` **diff 空**（§1.3 Non-goal 兑现）。
- 公开面：`apidump --diff` = 新增 7 项 / 修改删除 0 项；三既有 `port_flash_*` 与 bootctl 七签名对 tag `v2.28` **逐字相同**（`diff` 空，行号与文本在提交消息内）。
- CI：`.github/workflows/ci.yml` 85+/0− 单段新增（`cmake-integration` 七步，含"空 TU 无符号"的**对照面** `-gt 0` 防空 grep 假绿），既有 job 与步骤零改动。
- 库面：**六目录 diff 空**（`core/algorithm/sys/protocol/drivers/debug`，HC-1）；`storage/et_bootctl.{c,h}` 与 `port/*` 属计划允许面。

## 6. 风险与遗留

1. **REQ-7 真固件双单未闭合**（`BLOCKED-2`）：分区面已达成且实测，缺的是**把 88 KB 真固件传进 B 槽**的可靠链路；三条修法（1K 块型 / 传输期静默心跳日志 / 降波特率）与判据已在申报件 §3。**M4-④ 判据本版不达成**。
2. **`AT+BOOTINFO` 归因陷阱**：烧录只写应用区，参数区状态扇区不擦 ⇒ 三字段可为历史残留。任何版本若以该读数作 confirm 证据，须先证明状态扇区在本次烧录中被擦除或 `abandon` 过（本版已在 §2 AC-8 与实机记录 §19.2/§19.5 双处写死此限定）。
3. **头注自相矛盾两处未修**（`et_frame.h` / `et_atcmd.h`）：属 `protocol/` 零 diff 面，转 v3 候选 A；在此之前并发表对这两行标「冲突」。
4. **板侧工程存在第二份 `port.h` 的历史**（已移出源码树）：同步门（`diff -rq` / sha256）**看不见**源码树内的遮蔽副本 ⇒ 若再出现同类副本，构建会红而同步报告绿；候选缓解 = 同步面加"头文件搜索路径唯一性"探针（属上位机工具面，v3 池）。
5. **clang 侧 15 条警告**（10 CRT deprecation + 4 set-but-not-used + 1 self-assign）未清：本版只如实登记，零警告门仍以 gcc 为准（计划 P3-3④ 明示不在本版义务）。
6. **发版三件（merge main / tag `v2.29` / CI + Release）属 Owner**：本版计划 HC-0/§1.3 未授权执行者代做（v2.27 的代做授权**不外溢**到本版），本档 §7.6 给出复现与读数口径。

## 7. 复现与自检表

### 7.1 N（docsync 断言数）推导与实测三方一致

公式（`CO-10(v2.28)` 修正版，README 清单⑧）：`N = 上版实测 + 当版交付文档数 + 当版 rN 文件数 + 1（CHANGELOG 链） + K（本版新增断言） ± 当版占位符拦截面变化`

1. 上版终态实测 = **378**（`versions/v2.28/DELIVERY-r2.md` 行首声明 378/378；主档 375 为其前态）。
2. 当版交付文档数 = **+1**（本档进入「覆盖率行」循环）。
3. 当版 rN 文件数 = **+0**（是否开 rN 轮由审核方判定，执行者不预判）。
4. CHANGELOG 链 = **+1**（`[2.29.0]` 条目）。
5. `K` = **+3**（P0-2 支持矩阵、P1-2 双源一致性、P3-1 并发表逐名对账）。
6. 占位符拦截面变化 = 预置 **−2**（上版两份交付档 `versions/v2.28/DELIVERY.md` 与 `DELIVERY-r2.md` 随版本切换到 2.29 **退场**各 −1；本档进场 +1 已在第 2 项计入）——**该项由本档实测证伪，正确值 = −1**，逐项对账见下段。

**预置值与实际值的两时点读数（本版的自暴露，如实登记）**：按 `CO-10(v2.28)` 公式逐项预置 **N = 378 + 1 + 0 + 1 + 3 − 2 = 381**，终态实测 **382** ⇒ **预置失准 1**。失准项 = 公式第 6 项（占位符拦截面变化）取 **−2**，实际 **−1**：`docsync.sh:670-684` 的占位符循环按 `versions/v${VMM}/DELIVERY*.md` **当版 glob** 展开，v2.28 终态该循环 = **2 槽**（主档 + `DELIVERY-r2.md`），本版 = **1 槽**（本版无 rN 档），差 = −1；我按「上版两份各 −1」计，把「本档进场」重复计了一次退场（该进场已单独计入第 2 项覆盖率行循环）。

**实测分解（不靠公式，靠两棵树逐标签多重集对账）**：`git worktree add --detach ../et_wt_v228n v2.28` ⇒ 在该树 `sh tools/docsync.sh` = **pass=378 fail=0**（独立复核，主树零写入），与本版输出各取 `^(ok|FAIL)` 行、数字归一为 `#` 后 `sort | uniq -c` 做多重集 `diff`，**逐项差 = +4** 且仅下列五项命中（其余标签逐值同）：

| 差 | 断言标签（数字归一形态） | 本版读数 |
|---|---|---|
| +1 | `CHANGELOG 含 #.#.# 条目`（循环逐条目） | 29 → **30**（`[2.29.0]`） |
| +1 | `CMake glob 收到的每个库源文件都在 Makefile *_SRC 点名` | **1**（K P1-2） |
| +1 | `README 支持矩阵节在位` | **1**（K P0-2） |
| +1 | `并发契约表与模块集合逐名一致` | **1**（K P3-1） |
| +1 / −1 | `交付文档覆盖率行: versions/v2.29/DELIVERY.md` **+1**；`交付文档占位符拦截: versions/v#.#/DELIVERY-r#.md` **−1** | 覆盖率行 44→45（本档进场）；占位符 2→1（本版无 rN 档） |

⇒ **修正式（供下一版沿用，本档不代改 README 清单⑧条文）**：`N = 上版实测 + K + 1（CHANGELOG 当版条目） + (1 + rN_当版)（覆盖率行，逐档 1 槽） − rN_上版（占位符循环 = 当版档数 1+rN，差即 rN_当版 − rN_上版）` ⇒ 本版 `378 + 3 + 1 + 1 − 1 = 382` **与实测逐值同**。

**终态三方一致**：`docsync` **pass=382 fail=0**（[自指] expect = 末行 pass 总数 382）、`docs/architecture.md` 机制门行 = **382 断言**、本档行首声明 = **382/382** ⇒ 声明 / 门行 / 实测三方一致。**中间态如实登记**：P0-2 落地起转红（对已发布 v2.28 r2 档声明 378 不可回改，HC-4），P4-1 基线滚动后 **376/4**、P4-3 登记链收口后 **377/3**（两值取自带这两笔的提交消息实测），本档落盘且 README 链接行与 [评审记录] 面条回填后 **381/1**（唯一红 = 本档行首声明当时仍为占位形态），本批收口 = **382/0**——该红为机制预期的中间态，非失准。

### 7.2 复现命令（终态全绿序列）

```sh
sh tools/docsync.sh                                   # 期望 382/0
sh tools/apidump.sh --check                           # 期望 rc=0
sh tools/apidump.sh --diff                            # 期望 新增 7 / 修改删除 0
sh tools/sizecheck.sh                                 # 期望 pass=3 fail=0
make WERROR=1 test && make test-g4 && make test-1k && make test-tab   # 526/526/527/533 全绿
make ex                                               # 六配方各 PASS, rc=0
sh tools/docbuild.sh                                  # 两板构建 rc=0 警告 0
cmake -B build-cmake -DCMAKE_C_COMPILER=gcc -DET_WERROR=ON -DET_BUILD_EXAMPLES=ON && cmake --build build-cmake && ctest --test-dir build-cmake   # 6/6（gcc 前置：CMake 默认会选到 clang 并命中已登记的 -Wself-assign，评审 CO-3）
make test-asan CC=clang ASAN_DLL_DIR=<LLVM asan 目录>  # ALL PASS rc=0（gcc 侧 cannot find -lasan 为已知对照面）
sh tools/gatecheck.sh 2.29 --verify versions/v2.29/DELIVERY.md        # 期望 verify PASS
```

### 7.2b 覆盖率行（`docsync` 交付复现表检查的承载；本版两面新增点名）

**测试与质量门覆盖面（四形态 + 两新增面）**：host 全量回归四形态 **526/526/527/533**（`make WERROR=1 test` / `test-g4` / `test-1k` / `test-tab`，各 rc=0，基线 520/520/527/521 各 +6 = P2-2 区域 6 例）+ **37 套件**（实测 = 本轮 `make coverage-build` 输出 `[Suite] … (N cases)` 求和 = 37 套 / 526 例，末行 `RESULT: ALL PASS (fail=0)`）+ `make ex` **六配方**各 PASS（v2.29 由五例扩至六例 = P0-3 `ex_min`）+ **CMake 双路径**（`add_subdirectory` 与 `install`+`find_package` 两形态各 5 行消费件可编可跑、`ctest` **6/6 Passed**、`-DET_MODULE_KV=0` 裁剪位形的符号读数 10→**0**，均带对照面）+ **板侧真分区**（`AT+REGIONS` 四条板侧读回 + 真固件打包 87,872 B ≤ 槽 131,072 B + 链接脚本 128K 护栏 + 五要素走单 rc=0；**真固件双单传输面未闭合** ⇒ 本行按「覆盖面含一红」如实写，不写成全绿）+ 机制门五项（docsync 382 / docref / apidump `--check`+`--diff` / sizecheck / docbuild）+ `make WERROR=1` 零警告门 + ASan 双变体（本机 clang 23.1.0 经 `ASAN_DLL_DIR` 一条命令可达，gcc 侧 `cannot find -lasan` 为已知对照面）+ gcov 实测（本轮 `gcovr --print-summary --txt-summary --fail-under-line 85 --xml`，**默认全仓口径**：lines **97.4%** = 11,391/11,693、functions **99.3%** = 1,146/1,154、branches **57.9%** = 6,059/10,470，rc=0 即 ≥85% 门通过；取数时点 = 本档落仓笔 `17c4df8`，`rm` 掉 `.gcda/.gcno` 后重跑而非沿用中间读数）。口径注记：v2.27/v2.28 的覆盖率行为**定性承载**（本版沿用其形态并补两面），gcov 三条 = 本版新增的定量面。

### 7.3 登记链逐档点名（(d) 桶，逐文档非聚合）

本版对 (d) 的两批动作与**本档自身的桶**（逐值取自 §7.4⑧ 的 (d) 行，非本段自述）：① **P4-3**（`4b991fc`）对九档（`v2.20`~`v2.24` 五件平铺档 + `versions/v2.25~v2.28/DELIVERY.md` 四件）各 +1 行纯登记，token = 本版前十笔；② 但同一笔使这九档由 v2.28 终态的 **OK 9** 退化为 **未登记**——成因 = 登记笔**自身**的 SHA 无法在该笔内列出（自指不可预写），且 `0f9389e`（入仓批 = 快照本身，触碰 `BACKLOG.md`/`ROADMAP.md` 两个非白名单件）从未登记过；③ **P4-4b** 补登 `@0f9389e @4b991fc @17c4df8` 三 token 各一行（同一手法、纯登记提交 ⇒ 由 `docref.sh:176-184`「该笔对该档的增删行全部为登记行」路径豁免，不需自我 token），九档复绿；④ **本档自身**在终态判为「未登记实质变更」= **结构性未闭合**：本档落仓笔 `17c4df8` 是非白名单触碰面（README/architecture）而无法在本档内预写其 token，且定稿链粘贴笔亦触及本档 ⇒ 与「代登记」不可同时成立；该面已入 `docs/v3-candidates.md` 候选池 **D**（含两条候选修法，本档不代改门判定语义 HC-7）。其余 15 份「未登记实质变更」（= v2.28 终态既有 14 份 + 本档自身）与 9 份「缺声明行」、3 份「自指滞后」、9 份 OK 的分布，逐值取自 §7.4⑧ 的 (d) 行；本版只动上述九档与本档 ⇒ 其余不代其登记（沿 v2.28 手法与 §6 零伪造纪律）。逐档 emit 机读值见 §7.4⑧。

### 7.4 ⑧ 机读值（`--emit` 逐字粘贴，禁重打）

```
[gatecheck:emit] (b) 全量 = 51 files / 2811 / 117
[gatecheck:emit] (b) 自排除 = 50 files / 2627 / 117 (口径 = -- ':(exclude)v2.29开发交付__*.md :(exclude)versions/v2.29/DELIVERY*.md')
[gatecheck:emit] (d) 全集 36 份 → OK 9 / 未登记实质变更 15 / 自指滞后 3 / 缺声明行 9 / 其他 0
[gatecheck:emit] (a) CHANGELOG [2.29.0] 节日期 = 2026-10-20; 全文日期计数 = 2026-09-29×5 2026-10-01×3 2026-09-25×3 2026-09-24×3 2026-09-12×3 2026-10-20×2 2026-10-02×2 2026-09-30×2 2026-09-27×2 2026-09-09×2 2026-09-06×2 2026-10-10×1 2026-10-09×1 2026-10-08×1 2026-10-07×1 2026-10-06×1 2026-10-05×1 2026-10-04×1 2026-10-03×1 2026-09-26×1 2026-09-19×1 2026-09-11×1 2026-09-08×1 2026-09-07×1 2026-09-05×1
[gatecheck:emit] 登记 token = v2.10开发交付__挂账清偿与板侧闭环.md=22 v2.11开发交付__勘误批与轨道收口.md=3 v2.12开发交付__勘误清偿与判据自洽.md=4 v2.18开发交付__碰撞面根治实施与记法机制化.md=4 v2.19开发交付__勘误更正与时点纪律收口.md=5 v2.20开发交付__定稿后复测机制与基线口径明示.md=75 v2.21开发交付__勘误收敛与封闭枚举.md=70 v2.22开发交付__非代码清欠与复测脚本化.md=66 v2.23开发交付__引用机检闭环与纪律块重组.md=63 v2.24开发交付__勘误收敛与verify时点固化.md=57 v2.9开发交付__模板口径澄清与板侧同步窗口-r2.md=1 v2.9开发交付__模板口径澄清与板侧同步窗口.md=54 versions/v2.25/DELIVERY.md=51 versions/v2.26/DELIVERY-r2.md=1 versions/v2.26/DELIVERY.md=46 versions/v2.27/DELIVERY-r2.md=3 versions/v2.27/DELIVERY.md=41 versions/v2.28/DELIVERY-r2.md=4 versions/v2.28/DELIVERY.md=20
```

### 7.5 复测预期与 tag 后红

`--verify` PASS 的时点 = 定稿链末笔（身份见 §4 首行）；tag 之后 verify 转红 = **预期态**（区间右端随 HEAD 漂移，§4 冻结终点不再等于 HEAD），非缺陷。

### 7.6 快照与触发面

`snap-2.29-start` = **`0f9389e`**（入仓批末笔，HC-2 无 `v` 前缀形态）；打后实测触发面：`gh run list --limit 3` 无新 run、`gh release list` 无 2.29 条目 ⇒ **快照不推送即不触 CI/Release**（沿 v2.27/v2.28 实测形态）。本版基线 = `tag v2.28` = `ef339dd`。

## 8. CO 承接表（本版输入 → 处置 → 去向）

**两源差集对齐（两口径并列，禁单值化）**：计划 §1.1 = **26 行** ↔ `docs/v3-candidates.md`「v2.29 复评记录」= **26 行**（差集 = ∅，逐行同序，实测行号 1…26）↔ 本档 §8 = **26 行**（同源逐行誊录，无归并无缺载；与 SSOT 差集 = ∅）。

| # | 输入 | 处置 | 去向 |
|---|---|---|---|
| 1 | `CO-1(v2.28)`-P `board_window.py` abort 判据混算 | 已关闭（复核） | r2 R6 清偿；本版残差读数三次实跑在档 |
| 2 | `CO-2/3/4/5/6′/9/11(v2.28)`-M 七条 | 已关闭（沿用 r2） | v2.29 未重开 |
| 3 | `CO-7(v2.28)`-P 缺体量/容量维 | 纳入（两条） | 模板 §7④-bis + 本版真分区实测 87,872 ≤ 131,072 |
| 4 | `CO-8(v2.28)`-P 内存门可达性 | 纳入（路径①'） | P3-3 `ASAN_DLL_DIR` + 三处勘误 + 15 条警告登记 |
| 5 | `CO-10(v2.28)`-M 评审侧时点纪律 | 纳入 | 两模板双时点条文；本档 §4 末段执行首例 |
| 6 | `CO-12(v2.28)`-M gatecheck 区间右端 | 暂缓 | 维持 v3 池（本版未改门判定语义） |
| 7 | `RC-1` 串口异常未捕获 | 纳入（P3-2） | 有界重试 + abort + 退出码 2/1 + 途中断口保留读数 |
| 8 | `RC-2` docref 提取不对称 | 暂缓 | v3 池；本版沿用现网规避纪律 |
| 9 | `RC-3` 申报件白名单巧合豁免 | 纳入（P0-5） | 两模板「白名单归属须显式」条文 |
| 10 | `RC-4` §8 缺「新增破坏性候选」段 | 纳入（P4-4） | 见本节末段（RC-4 义务兑现） |
| 11 | `RC-5` 末笔身份散写九处 | 纳入（P0-4④） | 模板条文 + 本档 §4 首行单点承载（首例） |
| 12~19 | REQ-7 / REQ-10 / REQ-11 / REQ-12…REQ-17 / REQ-18 | 见计划 §1.1 同序行 | 已交付 / 部分 / 暂缓逐条点名 |
| 20~21 | M3 结案 / M4 新建 | 纳入（P0-6） | ROADMAP 已结案段 + M4 四判据 |
| 22~26 | 跨版链 / ch32x035 / f103 / G474 窗口义务 / 状态位归属 | 暂缓（维持）/ 纳入（窗口义务已执行）/ 不代改 | 触发逐条不变；状态位由 Owner 复评改判（铁律 9） |

**新增破坏性候选（RC-4 落点义务）**：**无**。本版公开面为**追加型**——`apidump --diff` 对 v2.28 与 v2.27 两基线均 = 新增 7 项 / **修改删除 0 项**；`et_bootctl_cfg_t` 尾字段追加不改既有偏移且无 ABI 边界（源码分发），已在 `API_STABILITY` §5.1 登记；区域 API 的三签名与 bootctl 七签名逐字不变（HC-6 判据）。故本节按"无候选"如实写"无"，不虚构破坏面。
