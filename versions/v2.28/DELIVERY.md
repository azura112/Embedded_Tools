# v2.28 DELIVERY：板侧窗口自动化、真固件升级链判定与自测内存防线（功能版）

> 本版 docsync：**375/375**（终态实测，行首声明供 [自指] 断言对账；推导逐步式见 §7.1，与 `docs/architecture.md` 机制门行同值）
> 交付日期：2026-10-10（终态提交日）｜ 执行者交付，输入 = `versions/v2.28/PLAN.md`（已批准-冻结，出口 A 功能版·全量档）+ `versions/v2.27/REVIEW.md`（r1，有条件通过）+ `versions/v2.27/REVIEW-r2.md`（r2，**通过**，0 阻塞项，新出 `CO-11…CO-13(v2.27)`）
> 首行声明（§6 预申报形态）：实际起点 = `25d8346`（本版入仓批：`versions/v2.28/PLAN.md` + BACKLOG/ROADMAP 规划者预改 + `BLOCKED-1__…md` 转「已解除」随批入库）；**≠ 基线 `tag v2.27` = `a02f98a`**，差一笔发版后补丁 P-1（`5d33d2f`，在基线内、快照前）⇒ 对账区间 `snap-2.28-start..HEAD` 为纯版本任务面。
> 开工前置（HC-0）三项实测值 = AC-0 行承载；`versions/v2.28/REVIEW.md` **尚不存在**——阶段 4 由审核方（≠执行者）独立会话产出，本档为其输入。

## 1. 交付摘要

主题 = **把板侧窗口从「手工规程」变成「一条命令」**（M3-⑤）+ **真固件升级链的可达性判定**（REQ-7）+ **自测面内存防线**。P0 条文与机制批（G0 零库面）：`.gitattributes` 入库根治幻影 ` M`（`CO-6`，index 侧**零 blob 改写**、工作树 78 个 `w/crlf` 归零）；两模板**五处条文**（登记行 carve-out / 结构体字段追加的允许面↔义务交叉引用 / 连写串判据作用域与门内历史注释豁免 / 工作区洁净判据以 `git diff` 为准 ×2 文件）；`templates/DELIVERY.md` 偏差表**四列→五列对齐**（`RC-1`）；`docsync` 新增「`API_GUIDE ## 10` 开关枚举 ↔ `et_config.h` 开关集合」**逐名对账**断言（补齐 v2.27 只闭数量一半的后半，反向探针红可复现）；README 清单⑧ 的 N 预置公式补 **K 项 + ±占位符拦截面**项（本版即新公式首例自证）；v3 池新增 `CO-9(v2.27)`-① 入池条目；ROADMAP 升格 **M3-⑤** 并把项目状态行按 v2.27 发版实果改写。P1 库面一致性：`et_metrics_format` 补 `slots == NULL` 守卫（本版唯一 **CO-P**，早退返回 0，与同族守卫逐字同形，签名/返回值语义不变）+ `metrics.null_safety` 用例内扩展构造态覆盖 + `et_metrics_bind` 生命周期契约注记 + 两处口径文档化（hist 窄化落 **under** 桶、截断提示的必要非充分）。P2 板侧窗口（G474）：同步（`diff_rq` 九行全 OK + sha256 72 件双侧一致）→ 构建 0 warning → 烧前 ELF 版本串机检 PASS → SWD 烧录 verified → 走单五项全绿（含 `loop_miss=2` 板上第一手失准读数与 `AT+BOOTINFO` 三字段前后基线）；**REQ-7 真固件 IAP 双单 = 受阻未达成**（当版真固件 48140 B vs 板侧 B 槽 = 参数区单页 2048 B，两单传输均 `receiver CAN at block 17`；申报 `BLOCKED-2`，AC-8 按未达成登记，不以合成镜像冒充）。P3 REQ-5：**`tools/board_window.py`** 五要素 + 三模式（`--dry-run` 干跑零产物探针、机检红 abort 不烧录、`--full` 与手工 §16.2 **逐要素对照 13 行**）+ Python 3.12.4 / pyserial 3.5 依赖声明两处 + `docsync` 工具断言。P4：基线滚动三件事（`API_INVENTORY_v2.27.md` 由 worktree 于 tag 真生成 461 项 + BASE 两处切 v2.27 + 发版点复读 **0/0/0**）、数字回刷同批（体积三行实测 `+8 B`/`+12 B`/`+8 B` 且声明不互推、用例数 **520 维持**、CHANGELOG `[2.28.0]`）、登记链收口、本档。

**本版两条真实发现（非计划内、按 HC-8 如实登记）**：① 发版后补丁 P-1（`5d33d2f`）——v2.27 的 `metrics.dump_cmd_through_shell` 把换绑句柄放在内层块作用域，出块后经 `AT+METRICS` 读越域栈对象，**Linux ASan 判 `stack-use-after-scope`**（本机 MinGW 无 libasan ⇒ 四形态 520 全绿对此类缺陷天然不可见，同族 = v2.5 `va_list`「只在 Linux 报警」）；② D-1（`19a72c8`）——四件已发布文档的 Windows 路径与转义串被历版写盘管线求值成裸控制符（`实机记录` 因此长期被 git 判二进制、证据目录路径不可 grep），本版 P0-3 的行尾归一使其成为必清前置。

## 2. AC 自查表

> 时点口径：除注明外，本表数值 = P4-5 定稿时点实测（终态提交上）；日期 = 终态提交日 2026-10-10。计数逐值标注口径（HC-8④：diff 行数 / 非空行数 / 行计数 / 出现次数四口径不混用）。

| # | 结论 | 证据 | 任务 |
|---|---|---|---|
| **AC-0** | **通过** | 开工前置三项实测：① `git merge-base --is-ancestor db655b6 main` → **exit 0**（`main` 含 r2 末笔）；② `git tag --list v2.27` → `v2.27`，`git rev-parse --short 'v2.27^{}'` → **`a02f98a`**（非 `db655b6`，`git cat-file -t` = `commit` 轻量标签沿 v2.26 形态），`git diff --shortstat snap-2.27-start v2.27` = **36 files / 2770 / 64** 与 v2.27 主档 ⑧ (b) 锚同值；③ CI run `38046749929`（main push）七 job **全 success**（`unit-tests ubuntu/windows`、`coverage`、`cross f103/g474`、`renode smoke`、`docbuild`），`gh release list` = **v2.27 Latest**，Release 附件 **4** 件（`demo.exe` / `SHA256SUMS` / `stm32f103_demo.bin` / `stm32f103_demo.elf`），`git branch --list "dev/*"` 于本版分支建立前 = 空（`dev/v2.27` 已删，`Deleted branch dev/v2.27 (was 68ca5bd)`）。发版步执行主体 = **Owner 2026-10-10 授权执行者代做**（归因载体 = `versions/v2.28/PLAN.md` 变更记录 + `BLOCKED-1__…md` §4，非执行者自裁） | HC-0 / §6 ① |
| **AC-1** | **通过** | `grep -c "^  - ⑤" ROADMAP.md` = **1**（新判据行 = 「板侧窗口一键化在位且有板侧实跑证据」，指向本版交付物 `tools/board_window.py` + 实机记录 §18.3 脚本产出 + 手工对照表）；裸 `⑤` 全文命中 4 行 = 判据行 + 达成状态/收尾注记/关联三行的指称（判据串取带行首缩进形态以自洽，`CO-13(v2.27)` ① 面的同族防呆）；项目状态行按实果改写并保留规划者预改语义（发版三件读数逐值入行）；M3 达成状态行「闭环待复评」→「实证在档且闭环已复评」而**不写终态**（⑤ 在途）；REQ-7 登记为 M3 关联项；numstat = 5+/4−（⑤ 行纯插入 + 四行行内改写） | P0-1 |
| **AC-2** | **通过** | 四处条文各 `grep -c` = **1**：`docs/开发计划模板.md`「登记行 carve-out」1 /「结构体字段追加版的允许面↔强制义务交叉引用」1 /「连写串判据的作用域与门内历史注释豁免」计划 1 + 评审 1 /「工作区洁净判据以 `git diff` 为准」计划 1 + 评审 1；`templates/DELIVERY.md` 偏差表表头列数实测 = **5**（`# / 偏差 / 说明 / 处置 / 计划面外授权`）且「五列形态」1 命中。numstat 与形态 = `docs/开发计划模板.md` **5+/1−**（区间累计口径 `git diff --numstat snap-2.28-start..HEAD -- 该件` = 5+/1−；形态 = **四行纯插入 + 一行严格前缀 1:1 替换 ⇒ rewritten 0**——前四项 = P0-2 四条条文，第五项 = P0-5 在「判据自洽性检查」行尾做的严格前缀扩展 [v2.28-r2 T-5① 勘误，`CO-4(v2.28)`]）、`docs/评审记录模板.md` **2+/0−**（纯插入两行）、`templates/DELIVERY.md` **3+/3−**（列头 + 分隔 + 增强要素行行内替换，`wc -l` 28→28 不变）。`.gitattributes` 存在且「新增属性文件后」与「工作树归一后」两次 `git diff --numstat` 均 = **空**（零 blob 改写；`git ls-files --eol` 由 `i/lf w/crlf` **78** → **0**，`i/lf w/lf` 385）；幻影探针 = `git status --porcelain` 与 `git diff --name-only` 差集 **∅**，且对全部 385 个跟踪件 `utime` 刷 stat 后复探仍差集 ∅（`CO-6(v2.27)` 复现路径失效）。三门复跑同值：`docsync` **372/0**（P0-3 时点）、`docref` 无参 = FAIL 缺声明行（预期态同串）、`apidump --check` exit 0、`sizecheck` 3/0。**计划字面偏离（判据优先，见偏差 3）**：归一不靠 `git checkout -- .` 单独达成（属性生效后 git 仍视 CRLF 工作树为 clean），改取「删 78 件 + checkout 重检出」 | P0-2 / P0-3 |
| **AC-3** | **通过** | 断言在位 = `tools/docsync.sh`「API_GUIDE §10 开关集合 == et_config.h ET_MODULE_* 集合」，正向 `ok … (32 名逐名一致, P0-4)`；**反向探针红** = `tar` 展开工作树副本至 `/tmp/probe-p04`（主树零写入）后 `sed -i 's| / MEDFILT||' docs/API_GUIDE.md` → 该块 **FAIL** 并逐侧打印集合；同文件既有断言零改动（`numstat` = **35+/0−** 纯插入，判定语义零变更，HC-7）；architecture 机制门行随 N 滚动（§7.1 推导，终态与实测同值） | P0-4 |
| **AC-4** | **通过** | `grep -c "当版占位符拦截面变化"` = README **1** + `docs/开发计划模板.md` **1**（两处均携补全式 `N = 上版实测 + 当版交付文档数 + 当版 rN 文件数 + 1（CHANGELOG 链） + K（本版新增断言行数） ± 当版占位符拦截面变化`）；**本版即新公式首例自证** = §7.1 六步推导（372 + 1 + 0 + 1 + **K=2** − 1 = **375**）与终态实测 **375/375** 逐项一致（不一致即公式仍缺项，本档按实测承载） | P0-5 |
| **AC-5** | **通过** | `debug/et_metrics.c` 的 `et_metrics_format` 体内含守卫 `if ((m != NULL) && (m->slots == NULL))`（`grep -c` 行计数 = **1**，形态与同族 `m_find:116` / `iter:277` / `reset` 逐字同形）；扩展用例在 `test_metrics_cases`（`metrics.null_safety` = `mt_count_and_null_safety` 内新增构造态 `{slots=NULL, cap=4, count=2}` 的七处断言 + 已 init 空表对照断言）；`make WERROR=1 test` = **RESULT: ALL PASS**、`debug/metrics => pass=16 fail=0`；四形态 `test` / `test-g4` / `test-tab` / `test-1k` = **ALL PASS ×4**（520/520/527/521）；`sh tools/apidump.sh --diff` 对 v2.27 基线 = **新增 0 / 修改删除 0**（HC-6 的 0/0/0 形态）；签名字符串不变（`--check` exit 0）。**负向物证**：`/tmp/probe-p11` 临时树删去守卫三行后 `metrics.null_safety` 以 **exit -1073741819（0xC0000005）** 崩溃 ⇒ 守卫与断言互为必要（本机 MinGW `make test-asan` 链接期 `cannot find -lasan`，Makefile:102-104 既有声明 ⇒ ASan 面 Linux-only） | P1-1 |
| **AC-6** | **通过** | 两处口径文字命中：`grep -cF "under** 桶"` = `debug/et_metrics.h` **1** + `docs/API_GUIDE.md` **1**；`grep -cF "必要非充分"` = 各 **1**；实测点标注在文（窄化 `et_metrics.c:244`、写入器语义 `et_metrics.c:69-75`）。语义零改动可核 = 实现文件 diff 仅 P1-1 守卫一处（`git diff --numstat snap-2.28-start..HEAD -- debug/et_metrics.c` = 7+/0−），头注 `6+/1−` 全为注释行；并发标注零滚动 = `🔒ISR-safe` **4** / `🏠MAIN` **12**（与 v2.27 AC-12 行计数同值） | P1-2 |
| **AC-7** | **通过** | 实机记录 **§18.1** 在档（节号落笔前 `grep -n '^## '` 实测末节 = §17 ⇒ 取 18，未写死）含：同步 `diff_rq.txt` 九行全 OK + `sync_sha256_lib/board.txt` 各 **72 件**双侧一致、板侧 MINOR 27→28；构建 exit 0 + **warning 0** + FLASH **86424** / RAM **8048**（v2.27 = 86404/8048 ⇒ **+20 B**，全量 ELF 层面）；烧前机检集合 `['13.3.1','2.28.0','2.42.0','4.4.0']` 命中 2.28.0 = PASS（干扰串逐条标注）；烧录原文 `Download verified successfully` + `MCU Reset`；走单原文五项 = `ver=2.28.0 boot=34` / `sw=32` + 32 行 / `SELFTEST: 22/22 PASS` / MBRD 三形态 `req=1/2/3 resp=1/2/3 … disc=0` 三行齐 / `METRICS 11/12 drop=0` 含 **`loop_miss=2`**，另 `AT+BOOTINFO` 前后 `staged=1 confirmed=1 attempts=1` 逐值同、`rx=193 ore=0`、PC 侧 `served=3 crc_err=0 mismatch=0`；`port/stm32g474/README.md` 同步态原式 `grep -c "已同步 2.27.0 并重烧"` = **0**（改写为「于本版窗口同步至 2.27.0（该版时点态；v2.28 已再同步至 2.28.0）」，历史语义保留）；`sizecheck` = **pass=3 fail=0**（v2.28 三行实测 `25608/24/1512`、`38056/24/3792`、`25984/28/1512`）；证据落盘持久路径 = 板侧工程 `build/v2.28-evidence/`（九件 + 驱动两件，库外） | P2-1 / P2-2 |
| **AC-8** | **未达成（受阻，见 `BLOCKED-2`）** | 真固件双单**未跑通**：产物与打包面全部达成（Release preset FLASH **48140 B** 0 warning、`objcopy` 出的 `g474.bin` 内版本形串**仅** `2.28.0`、开工前该件为 2026-09-08 的 32064 B 旧产物已重建替换；`pack_image.py --ver 7/8` 两镜像各 **48172 B** = 32 B ETBI 头 + 同一 body，头实测 `img_size=48140 ver=7/8`，sha256 前缀 body `8e8b20b69ae0cbee` / odd `ad1bc11741c5df15` / even `c575d5c2cc7e3c4b`）；**传输两单均 `RuntimeError: receiver CAN at block 17`**（`xmodem_send.py` rc=1，各 1.4 s）⇒ confirm 单与回滚单均未取得 `BOOTINFO` 状态迁移证据。根因 = 板侧几何而非库/操作：`et_demo.c:72-74/85-86/286` 的 B 槽 = 参数区**单页**且 `xm_sink` 拒收跨界写、`et_port.c:27-28` G474 单页 = **2048 B**、`CMakeLists.txt:73-75` 参数区 = 16 × 2048 = **32 KB** ⇒ 48140 B 单槽与整区皆容不下。**未污染板上态** = 两单失败后 `staged=1 confirmed=1 attempts=1`、`ver=2.28.0` 逐值不变。**判据形态遵 HC-10③④**（`--ver` 奇偶 = 打包参数，未表述为「版本号导致回滚」；三字段读数在案）；**未以 `AT+SIMUPGRADE` 合成镜像单充当真固件单**（REQ-7 动机原文即「合成镜像通过 ≠ 真实发布固件通过」，HC-8①）。解阻三径（板侧分区改造转 v2.29 / Owner 批准变更判据 / 撤条）交 A2 与 Owner 裁决 | P2-3（REQ-7） |
| **AC-9** | **通过** | `tools/board_window.py` 存在且被 `docsync` 断言点名（`ok 板侧窗口一键脚本入库(REQ-5/M3-⑤, v2.28 P3-1)`）；三模式行为可核验——**`--dry-run` 绝不烧录**：exit 0、证据目录**未创建**（`ls -d …/build/v2.28-evidence-script` → No such file or directory）、无 `flash.log`/走单件、CLI 未被调用，脚本对每个写盘点打印「dry-run: 不落盘 …」；**机检红 abort 路径**：`version_check()` 不过即置 `abort_reason` 且 `run()` 直接返回不进烧录分支（HC-10②），且本版有一次**真实自触发**——`--full` 首跑因 `STM32_Programmer_CLI` 在管道捕获下不回显而得 `verified=False` 主动中止（`flash.log` 0 字节、rc=0 却未判成功 = HC-8①防线生效），修法 = 子进程输出直接落文件后读原文；**`--full` 与手工逐要素对账表 = 实机记录 §18.3 十三行**（版本串 / 同步 / 构建与两种尺寸口径 / 机检集合 / 烧录 verified / `VER` / `BOOTINFO` / `FEATURES` 32 行 / `SELFTEST 22/22` / MBRD 三形态 `disc=0` / `METRICS 11/12 drop=0` 且 `loop_miss=2` **两路同值** / `RXSTAT ore=0` / 从站 `served=3`），差异逐条如实登记（boot 计数 34→37 = 两路间 3 次复位；`hb_runs` 复位起点差；`t=3/3/2ms` vs `2/2/2ms` 抖动；`rx` 随序不同）；依赖声明两处命中（getting-started 表行 + README 段落，Python **3.12.4** + pyserial **3.5** 实测值），`.github/workflows/*` 零 diff（不做 CI 化 = §1.3） | P3-1~P3-4 |
| **AC-10** | **通过** | `docs/API_INVENTORY_v2.27.md` 存在 = **461 项**，由 `git worktree add -f /tmp/wt227 v2.27`（解引用 `a02f98a`）内当版 `apidump --snapshot` **真生成**、禁手工编辑，worktree 随即 `remove --force` 清理；内容 sha256 = **`35e9ea72…07de3223`**、`git hash-object` blob = **`32d21d2e…3bac91d9`**；与 v2.26 归档 `diff` 命中 **45 行**且逐段解释 = 宏段 51→**55**（四宏）+ `ET_VERSION_MINOR 26→27` 一行 + 新章 `## debug/et_metrics.h`（17 函数声明）+ sched 三函数与 `et_features_*` = v2.27 AC-3 点名的 28 项同一集合（条目数 433→461 = 差 28 命中，无超出）；`tools/apidump.sh` BASE = v2.27 **两处实测**（`:15` 头注 + `:227` 默认值，`numstat` 2+/2−，口径遵 `CO-1(v2.27)` 禁写「三处」）；发版点复读 `--diff` = **新增 0 / 修改删除 0**（0/0/0 形态） | P4-1 |
| **AC-11** | **通过** | HC-5 清单逐项命中：MINOR **28** / `0x021C00`（`gcc -E -P -dM et_config.h \| grep ET_VERSION` = 2 / 28 / 0）；README 版本行 = `**v2.28.0**` + 基线名前滚 **×2**（`:194`、`:245` → `API_INVENTORY_v2.27.md`）+ 交付链接行（本档路径）；**用例数 520 维持**（本版用例零增减 ⇒ 无八处/两处的滚动面，`grep -rn` 终态全列点名 = §7.6）；四形态 **520/520/527/521** 实测；模块 **35** / 开关 **32**（且 P0-4 断言使集合对账有机检）；architecture 机制门行 = **375**（§7.1 推导，与终态实测同值）；CHANGELOG `[2.28.0]` 日期 = **2026-10-10** = 终态提交日；两 port v2.28 体积行 = `arm-none-eabi-size`/linker 实测回填（`+8 B` / `+12 B` / `+8 B`，构型差异声明「不互推」） | P4-2 |
| **AC-12** | **通过** | 终态门批次留痕 `build/final-gates-v228.log`：`docsync` **N/N fail=0**（行首声明与本节同值）、`apidump --check` exit **0**、`--diff` **0/0/0**、`sizecheck` **3/0**、`docref` 逐档点名判定（(d) 类**逐文档**列，禁聚合行）、`make WERROR=1 test` + 四形态 + `make ex` 五配方、`.github/workflows/*` 零 diff 实测；快照触发面 = 打 `snap-2.28-start` 同批 `gh run list --limit 2` 无新 run、`gh release list --limit 1` 仍 = v2.27 Latest、`git ls-remote` 无 `snap-2.28-start`（无 `v` 前缀 ⇒ 不触发 `release.yml`，HC-2 实测非先验）；**本版发版（merge main / tag `v2.28` / CI / Release）不由执行者代做**——HC-0 的代做授权面只覆盖 v2.27 发版本批（`versions/v2.28/PLAN.md` 变更记录明载「本版发版步不随之放宽」） | P4-2 / P4-5 |
| **AC-13** | **通过** | ⑧ 五锚行 = `sh tools/gatecheck.sh 2.28 --emit` **逐字粘贴**（§7.4 行⑧，禁重打）；`--verify` 绿态单点 = 定稿链末笔；tag 后 verify 红 = 预期态与 §7.5 逐字吻合；(d) 全集 = 本版 DELIVERY 进入后由 `--emit` 机读承载（计划预置 33 + 1 = **34 份**，实测分类值以发版点机读为准，禁预抄）；token 机读与 §7.3 一致 | P4-3 / P4-5 |
| **AC-14** | **通过** | G0 窗口谓词（`tools/docref.sh` / `gatecheck.sh` / `sizecheck.sh` + 库面六目录 + `et_config.h` + `Makefile` + `apidump.sh` + `.github/` + `port/` 代码面 + `examples/`）于 G0 终态提交 `44deb69` 实测 = **空**；`tools/docsync.sh` 显式排除并归因（P0-4 计划内新增 K 行）；本批触碰面实测非空 = `.gitattributes`（新增）/ 两模板 / `templates/DELIVERY.md` / `README.md` / `docs/v3-candidates.md` / `ROADMAP.md` / `docs/getting-started.md` / 四件已发布文档（D-1 复原面，HC-4 放行）并逐件写明归因；五库目录 + `Makefile` + 其余三门 + `tools/*.py`（除新增 `board_window.py`）零 diff | P0 / §6 ③ |
| **AC-15（本版新增判据面，HC-10⑤）** | **通过** | 板侧红 = 真实发现走 BLOCKED 而未改库面绕过：REQ-7 受阻后 `storage/` `port/` `et_bootctl` 契约零改动（`git diff --numstat snap-2.28-start..HEAD -- storage/ port/` 仅 README 文档行）、未扩参数区、未动链接脚本；脚本自发 `verified=False` 中止亦未改判据凑绿 | HC-10⑤ |

## 3. 偏差记录

> 五列形态（`templates/DELIVERY.md` P0-2③ 对齐后的本版首例）。「计划面外授权」列 = 评审条件清单驱动的门新增/门面变更来源；diff 行数 / 文件枚举类声称一律按值标注口径（`CO-7(v2.26)` 链）。

| # | 偏差 | 说明 | 处置 | 计划面外授权（来源 = REVIEW-rN T-n） |
|---|---|---|---|---|
| 1 | **已发布文档内容面被触碰**（四件：`移植stm32实机记录.md` / `CHANGELOG.md` / `docs/v3-candidates.md` / `port/stm32g474/README.md`） | HC-4 原文 = 本版对已发布档的实质变更面「仅登记行 carve-out」；实测需要 = 这些件里的 `\v`/`\b`/`\r` 被历版写盘管线求值成**裸控制符**（`实机记录` 因此是全仓唯一 `i/-text`，证据目录路径 `build`+VT+`2.27-evidence` 无法 grep），且 P0-3 的行尾归一要求文本分类正确 | 走 **HC-4 放行**并登记本条：`19a72c8`（D-1）字节级复原 + 逐处 count 断言、零数值声称变化、`docsync` 372/0 同值；重建依据 = 行标签自身与 `last=` 十六进制解码，余一处不可重建者（§16.4 UART 旁注）**未修**并如实登记 | 无（本版门改动全在 HC-7 授权面内） |
| 2 | **本版自触发同族缺陷一次**（`14d8f12` 的 CHANGELOG 段） | P4-2(b) 写 `[2.28.0]` 段时，正文内引述的四处转义字面量再次被求值成裸控制符（0x0B ×1、0x0D ×2）并把该 markdown 行截断成三行 ⇒ `CHANGELOG.md` 亦落 `i/-text` | 即时复原 `91f8493`（`chr(92)` 拼字面量 + 并回一行）；实测复原面 = 裸控制符 0 命中、`grep -c "build.v…-evidence"` = 1、`docsync` 371/2 与 `sizecheck` 3/0 不变；纪律沉淀入本条与 §7.4 行⑧（文档写路径串一律正斜杠或 `chr(92)`），**不另立条文**以免范围蔓延 | 无 |
| 3 | **P0-3 的命令面偏离计划字面** | 计划原文「再 `git checkout -- .` 归一 77 个 `w/crlf` 文件」——实测属性生效后 git 仍视 CRLF 工作树为 clean（规范化后与 index 逐字节相等），纯 checkout 不足以归一 | 改取「删除 78 个 `w/crlf` 跟踪件 + `git checkout -- .` 重检出」达成同一终态（`w/crlf` = 0、`i/lf w/lf` 385、三门同值）⇒ 判据优先于条文措辞，逐值读数入 AC-2 | 无 |
| 4 | **脚本证据目录与计划 P3-1 字面不同** | 计划写 `build\v2.28-evidence\`（手工同目录）；实测同目录会令脚本 `version_check.txt` / `flash.log` / `build.log` **覆盖手工留痕**，AC-9 的「逐要素对账」即失去双份物证 | 脚本件落 `build/v2.28-evidence-script/`（`--evidence` 参数即为此设）；两目录并存、对照表逐行可核（实机记录 §18.3） | 无 |
| 5 | **REQ-7（P2-3 / AC-8）物理不可达 = 计划判据自洽性缺口** | 计划 §7②/§7④ 实测了 `pack_image.py --ver` 语义、奇偶与 `ET_VERSION_MINOR` 无关、真固件产物在位，但**未核「镜像体量 vs 目标槽容量」这一维**：实物 = 板侧 B 槽 2048 B（参数区单页，`et_demo.c` 三处 + `et_port.c` 强制 + `CMakeLists.txt` 16 页）vs 当版真固件 48140 B | AC-8 按**未达成**登记（不降格、不冒充、不借合成镜像凑绿）；申报 `BLOCKED-2` 并给三条解阻路径；建议把该维核验补进权威模板 §7② ——是否收进条文由 A2/Owner 判（同 `CO-13(v2.27)` 口径面，见 §8 与偏差 8） | 无 |
| 6 | **ROADMAP 项目状态行由执行者改写** | 规划者预改行写「v2.27 = 交付完成、发版未做 + `REVIEW-r2.md` 不存在」，而本版内发版三件已定着、复评记录已入仓 ⇒ 该行成为 falsehood | 按 AC-1 括注的授权形态执行（「若 A2 复评已出则按复评结论改写该行的闭环待字」），改写携实测读数与授权归因；**BACKLOG 的 REQ-6/8/9 状态位不代转**（该计划变更记录初行明示不代转，铁律 9） | 无 |
| 7 | **v2.27 发版由执行者代做（跨版事项）** | HC-0 原文「push/tag/Release 不由本 Agent 代做」；Owner 2026-10-10 选择「授权我代做发版」⇒ 代做 merge ff（`ec15356`→`68ca5bd`→`5d33d2f`）、tag `v2.27` = `a02f98a`、push、CI 复跑、Release Latest、删 `dev/v2.27` | 归因与读数 = `versions/v2.28/PLAN.md` 变更记录两行 + `BLOCKED-1__…md` §4；**授权面只覆盖 v2.27 发版，不覆盖本版**（本版发版仍归 Owner，见 AC-12）；代做期间 CI 抓出的 v2.27 测试面缺陷以 `5d33d2f` 补丁清偿并如实登记（发布物 v2.27 不动 = Owner 第二判定） | 无（Owner 直接裁决，非评审条件驱动） |
| 8 | **`CO-13(v2.27)` 未并入本版 P0-2** | 复评 §5 给出两择一（本版变更记录追加收录 或 v2.29 判定收录）；本版计划已冻结且 P0-2 任务面不含该两条文，Owner 本批只授权「发版代做 + R9 一笔」两项 ⇒ 执行者不自赋本版扩容 | 按第二通道处置并显式登记（禁静默）：`versions/v2.28/PLAN.md` 变更记录第 3 行；**本版新增证据补强**：AC-1 的 `⑤` 判据串若不取「带行首缩进」形态即被本文自命中（同族 = CO-13 的作用域面），已在条文与实测两侧都做了防呆 | 无 |

## 4. 提交清单（记录范围 = `snap-2.28-start..HEAD`）

记录范围 = **`snap-2.28-start`（= `25d8346`，本版入仓批末笔）..本提交（定稿链末笔）**，B1 口径；`git diff --stat <base>..<end>` 的三值由 §7.4 行⑧ (b) 片段逐字承载（禁手写凑数）。基线另注：`tag v2.27` = `a02f98a`（发版锚，非本版区间左端），入仓批之前的两笔（发版批 `68ca5bd`、发版后补丁 P-1 `5d33d2f`）属基线内、不入本版对账面。

`git diff --stat snap-2.28-start..8a1f060` = **33 files changed, 1802 insertions(+), 39 deletions(-)**（口径 = 全量含本档；自排除 32 / 1620 / 39 与 (d)/(a)/token 三面由 §7.4 行⑧ 片段逐字承载，禁手写凑数。终点冻结为主批定稿链末笔 `8a1f060` = tag `v2.28` 落点 ⇒ r2 批在其后的提交不入本区间，走「修订登记 @<sha>」放行。**[v2.28-r2 T-1② 形态归位，`CO-6′(v2.28)`]** 沿 v2.27 `3b96494` 先例；归位前本档缺该行 ⇒ 无参 `docref` 恒报「缺『记录范围 + diffstat』声明行」。清单行数与笔数两口径对账见本节末段注记。）

```text
25d8346 入仓批（= snap-2.28-start）: versions/v2.28/PLAN.md + BACKLOG/ROADMAP 规划者预改 + BLOCKED-1 转「已解除」(§1-§3 原样 + §4 解除记录)
19a72c8 P0 前置修复 D-1: 四件已发布文档裸转义控制符复原（偏差 1）
993a05e P0-3: .gitattributes 入库 + 工作树归一实测（偏差 3：判据优先于条文措辞）
feb3645 P0-2: 两模板五处条文 + templates/DELIVERY.md 偏差表五列对齐
44deb69 P0-1: ROADMAP M3-⑤ 升格登记 + 项目状态行按发版实果改写（AC-1 / 偏差 6）
0fc6133 P0-4: docsync §10 开关集合逐名对账断言（K=1）+ architecture 机制门行 +1
e03cc50 P0-5: README 清单⑧ N 公式补 K 项与 ±占位符拦截面项 + 模板同批口径句
59ad762 P0-6: v3 池新增「来自 v2.28 候选池登记」节（CO-9(v2.27)-①）
0415b05 P1-1: et_metrics_format 槽守卫 + null_safety 构造态扩展（正负两组实测）
1dd9db0 P1-2: observe hist 窄化与截断假阳性两处口径文档化
1a9edb0 P4-2(a): 版本宏 MINOR 28 / 0x021C00 + README/API_GUIDE 版本串 + API_INVENTORY 重生成（板侧先决）
f348239 P2-1/P2-2/P2-4 + P2-3 受阻申报: 实机记录 §18 + g474 同步态改写 + v3 池两行 + BLOCKED-2
9f1fb71 P3-1~P3-4: board_window.py（五要素/三模式/真实 abort 实例）+ 依赖声明两处 + 工具断言（K=2）
e647bee P4-1: API_INVENTORY_v2.27.md worktree 真生成 + BASE 两处切 v2.27 + 复读 0/0/0
14d8f12 P4-2(b) 一: 两 port v2.28 体积行实测 + README 基线名 x2 + CHANGELOG [2.28.0]
91f8493 P4-2(b) 二: CHANGELOG 自触发裸控制符即时复原（偏差 2）
8966b4d P4-4: versions/v2.28/DELIVERY.md 落仓 + architecture 机制门行 373→375（docsync 三方一致收口）
de5513d P4-4 补记: 本档 §4 清单补入落仓笔 8966b4d（numstat 1+/0− 纯插入，行数 +1 后即冻结——⑧ 回填笔取 1:1 行内替换，不再改本档行数）
591c249 P4-3 登记链收口（纯登记）: 八档历史 OK 交付文档各 +1 修订登记行，逐笔点名本版非白名单面（HC-1 登记行 carve-out 首例按新条文落地）
c02f6d9 定稿链 D2: 本档 §4 清单补入 P4-3 收口笔（numstat 1+/1− 行内替换，本档行数不变 ⇒ (b) 双口径锚值自本笔起对回填笔稳定）
（定稿链**四笔** = `de5513d` + `591c249` + `c02f6d9` + `8a1f060`——前三笔已逐笔具名于上，末笔 `8a1f060` = 本档 ⑧ 片段 1:1 回填笔与 `--verify` 绿态单点、**自指不列**（docref「清单不含交付文档定稿提交自身」同源自指纪律）；清单行数与笔数的两口径对账见下段注记。**[v2.28-r2 T-1① 勘误，`CO-6′(v2.28)`]** 原式作「定稿链笔两笔」且 `de5513d`/`c02f6d9` 未具名）
```

**清单对账注记（[v2.28-r2 T-1③] 按值标注两口径，`CO-6′(v2.28)`/`CO-12(v2.28)`）**：本块围栏行数 = **21**（行首具名 SHA **20** 个 = 快照 `25d8346` + 本版 19 笔；另 1 行 = 定稿链四笔合写注记行，SHA 不在行首 ⇒ 不入 docref §4 提取面）；区间笔数 = `git rev-list --count snap-2.28-start..8a1f060` = **20**（不含快照自身；含快照端点则 `snap-2.28-start^..` = 21）⇒ 清单行首具名 20 = 区间 20 笔中除自指末笔 `8a1f060` 外的 19 笔 + 快照 1，**差集 = ∅**（复现：`git log --format=%h snap-2.28-start..8a1f060` 与围栏行首 SHA 集求差）。当前 HEAD 若已含 r2 批与申报/评审件，`--emit` 的 (b) 随右端后移 ⇒ 本档 ⑧ 片段值 = `8a1f060` 定稿时点真值，不得以红态 HEAD 复跑值回填（`CO-12(v2.28)` 条文，r2 批的机读面另见 `versions/v2.28/DELIVERY-r2.md` §7.4 行⑧）。

## 5. 机制自证与门改动面

- **门改动 = 两处，均在 HC-7 授权面内（评审条件/CO 点名，判定语义零变更）**：① `tools/docsync.sh` P0-4 新增 §10 开关集合逐名对账断言（**35+/0−** 纯插入，既有断言零改动）+ P3-4 新增 `tools/board_window.py` 存在性断言（**1+/0−**）⇒ 本版对 `docsync.sh` 的 diff 合计 **36+/0−**，无一行既有断言被改写或删除；② `tools/apidump.sh` BASE 两处路径滚动（2+/2−，比对算法与退出码规则不动）。`docref.sh` / `gatecheck.sh` / `sizecheck.sh` / `bench.c` / 其余 `tools/*.py` / `.github/workflows/*` **零 diff**（`git diff --numstat snap-2.28-start..HEAD -- <六件>` = 空，逐件点名见 §7.6）。
- **反向探针（门自证可红）**：P0-4 断言 = 临时树删 `MEDFILT` 枚举行 → 该块 FAIL 并打印两侧集合（`/tmp/probe-p04`，主树零写入）；P1-1 守卫 = 临时树删守卫三行 → `metrics.null_safety` 以 `0xC0000005` 崩溃（`/tmp/probe-p11`）。两探针件用毕即删。
- **占位符拦截自证**：本档散文行零裸方括号占位符（`docsync` 文末「交付文档占位符拦截」对本档 = ok）；§4 清单末行的「本提交」为自指口径而非占位符（沿 v2.27 R8 先例）。
- **幻影 ` M` 与行尾面**：`.gitattributes` 生效后 `git ls-files --eol` = 385 全 `i/lf w/lf`；全量 `utime` 刷 stat 后 `git status --porcelain` 与 `git diff --name-only` 均空（差集 ∅）；本档与 §18 的落盘路径一律正斜杠形态并注记原因。

## 6. 风险与遗留

- **REQ-7 未闭环 = 本版最大遗留**，且遗留的是**判据前提**（板侧无真 A/B 分区）而非一次操作：升级链证据面仍只有 v1.8/v1.9 的合成镜像形态 + 本版 `BOOTINFO` 三字段可读性实证。下一步无论走路径 A（v2.29 板侧分区改造 + `et_bootctl` 槽语义放宽，属契约面）还是路径 C（撤条），都必须与 `docs/API_STABILITY.md` 的结构体/契约义务交叉引用同批（本版 P0-2② 条文即为此预置）。
- **asan 门的可达性**：本机无 Linux（无 WSL、MinGW 无 libasan，Makefile:102-104 与 README 边界段均已声明）⇒ 内存类缺陷的机检只在 CI ubuntu job 存在，而 CI 只在 push main / PR 时跑 ⇒ 「本地全绿但发版后 CI 红」是本版结构性形态（两次实例 = P-1 句柄越域、`CO-11` 族的假绿断言）。候选处置（不自行实施）：开 PR 触发 asan 的惯例、或把「发版前跑一次 Linux 侧内存门」写入模板 §7。
- **脚本与手工的分工面**：IAP 双单不在脚本要素内（REQ-5 原文列其为可选步、本版计划 P3-1 五要素亦不含）⇒ 脚本覆盖面 ≠ 板侧窗口全义务面，登记于 §18.3 末行。
- `docs/v3-candidates.md`「v2.28 复评记录」节 = **交付批的三落点第二落点**（沿 v2.25 / v2.26 / v2.27 三版实做形态：SSOT = 计划 §1.1，由执行者随交付批落仓，**非 A2 产出**），本版 f103 / ch32x035 的 v2.28 逐版状态由该节行 18 与两条常设条目承载 **[v2.28-r2 T-3 更正，`CO-2(v2.28)`：原式「复评由 A2 产出，非本档职责」与三版先例及本版计划 P4-4 相悖]**。
- 若本版后续产生 rN 轮：先读 `CO-8(v2.26)` 与 v2.27 P0-4 条文（`head -1` 机检定位切换在 v2.27-r2 已现网实例化，v2.28-r2 会是第二实例），并遵「rN 档存在时主档声明不再被机检」口径。

## 7. 复现与自检表

### 7.1 N（docsync 断言数）推导与实测三方一致

新公式（P0-5，本版首例自证）：`N = 上版实测 + 当版交付文档数 + 当版 rN 文件数 + 1（CHANGELOG 链） + K（本版新增断言行数） ± 当版占位符拦截面变化`

1. 上版终态实测 = **372**（v2.27 交付行首声明与 `docs/architecture.md` 门行、`DELIVERY-r2.md` 声明三方同值）。
2. 当版交付文档数 = **+1**（`versions/v2.28/DELIVERY.md` 进入「覆盖率行」循环；本版无 rN 档 ⇒ 无第二件）。
3. 当版 rN 文件数 = **+0**（v2.28 评审为通过与否不由执行者预判）。
4. CHANGELOG 链 = **+1**（`[2.28.0]` 条目的版本链断言）。
5. `K` = **+2**（P0-4 §10 开关集合逐名对账断言 + P3-4 `board_window.py` 存在性断言，均为纯新增）。
6. 占位符拦截面变化 = **−1**（上版 rN 档 `versions/v2.27/DELIVERY-r2.md` 在版本切换到 2.28 后退出该循环）。

⇒ 预置 **N = 372 + 1 + 0 + 1 + 2 − 1 = 375**；终态实测 = `docsync` **pass=375 fail=0**（`[自指]` expect 即末行前的 PASS+1），`docs/architecture.md` 机制门行 = **375 断言**，本档行首声明 = **375/375** ⇒ 声明 / 门行 / 实测**三方一致**。中间态如实登记：P0-4 落地起 `docsync` 转红（已发布的 v2.27 r2 档声明 372 不可回改，HC-4），P4-2(a) 版本宏后为 **369/3**，P4-2(b) 后 **371/2**，随本档与 CHANGELOG 落仓收口至 **375/0**——该红是机制预期的中间态，非失准。**取数链（同树 worktree 实跑，命令 = `git worktree add --detach <tmp> <sha>` + `sh tools/docsync.sh | tail -1`，用毕 `worktree remove --force` + `prune`）= `0fc6133` 372/1 → `9f1fb71` 370/3 → `1a9edb0` 369/3 → `14d8f12` 371/2 → 终态 375/0**；原式「369/**6**」= 把终态 fail 面配到中间时点的 pass（该笔断言总数 = 369+3 = **372**，6 在此笔不可能出现），属取数错配而非 [自指] 双诊断行口径问题（本笔 FAIL 文本行数 = 3 = fail 计数，两口径同值）——**[v2.28-r2 T-5④ 勘误，`CO-9(v2.28)`]**。

### 7.2 复现命令（逐条可跑）

```sh
sh tools/docsync.sh                                   # 期望 pass=375 fail=0（含 §10 集合对账 + 占位符拦截 4 行）
sh tools/apidump.sh --check; sh tools/apidump.sh --diff   # 期望 exit 0；基线 v2.27 新增 0 / 修改删除 0
sh tools/sizecheck.sh                                 # 期望 pass=3 fail=0（v2.28 三行）
sh tools/docref.sh                                    # 期望 FAIL 缺声明行（本版无 DELIVERY-rN，对象 = 本档）
mingw32-make WERROR=1 test; mingw32-make test-g4; mingw32-make test-tab; mingw32-make test-1k   # 520/520/527/521
python tools/board_window.py --mode dry-run --evidence v2.28-evidence-script   # 期望 exit 0 且证据目录未创建
sh tools/gatecheck.sh 2.28 --emit                     # ⑧ 五锚行取值命令（本档 §7.4 行⑧逐字承载）
sh tools/gatecheck.sh 2.28 --verify versions/v2.28/DELIVERY.md   # 期望 exit 0 @定稿链末笔
```

### 7.3 登记链（docref 判定与放行面）

- 本版对**已发布档**的触碰面 = D-1 复原四件（偏差 1，HC-4 放行）+ 登记行 carve-out（P4-3 对各 OK 档纯登记）；`docref.sh` 白名单形态未变（本仓 `*实机记录*.md` / `versions/*/REVIEW*.md` 等属良性面）。
- 无本版内已发布档的 修订登记 token 需求：v2.27 四件在 r2 批内已携登记行；本档对它们的触碰 = 偏差 1/2 承载面（内容复原非声称改写）。
- 修订登记（r2 轮，短语行不携 token，遵碰撞规避纪律；token 行另起）：v2.28-r2 批对本档 §2/§4/§6/§7.1 的实质变更与本轮门强制数值面走放行，来源 = `versions/v2.28/REVIEW.md` §6 清单 T-1…T-5 + T-7（载体 = `versions/v2.28/DELIVERY-r2.md`）
- 修订登记 @ab4d104 @78aee9c @be6a40f @1b03bd6 @f158d05：r2 批逐笔点名面 = R3（T-5 本档 AC-2 与 §7.1 两处 1:1 行内替换）/ R6（T-2 `tools/board_window.py`，非白名单）/ R7（T-3 本档 §6 复评职责句更正）/ R8（T-1 本档 §4 三笔具名 + docref 声明行归位 + 对账注记）/ R9（T-4 `README.md` + `tools/docsync.sh` + `docs/architecture.md` 375→378 三面，非白名单）；R1 评审记录入仓、R2 申报件更正章与 R10 本笔 = 白名单或纯登记豁免面，不点名
- 修订登记 @20e767a @057cd2e @3307a1b @cc6d1b7 @d701c41 @8718397 @68dd7dc @4f1487a @e752234 @2b54f62：**v2.29 P4-3 登记链收口（纯登记行，HC-1 carve-out）**——本版对已发布档的非白名单触碰面逐笔点名放行，列举形态 = `git log --format=%h snap-2.29-start..HEAD` 全 10 笔，采全列而非择要（沿 v2.28 消「同串异写漏列」的枚举纪律）。内容面触碰仅三类且均已入本版偏差表：① 门与工具面 = `tools/docsync.sh` 三条纯新增断言（K=3，`20e767a`/`057cd2e`/`68dd7dc`）+ `tools/apidump.sh` BASE 两处切 v2.28（`2b54f62`）+ `tools/board_window.py` 串口异常面（`d701c41`）；② 库面 = port 区域契约与三 port 实现 + `et_bootctl` 位形（`3307a1b`/`8718397`），公开面 `apidump --diff` = 新增 7 项 / 修改删除 0 项（追加型）；③ 数字回刷与文档面 = 版本宏 29（`cc6d1b7`）、用例数 520→526 五处 + 两 port v2.29 体积行（`cc6d1b7`）、Makefile 十分钟样例接线（`20e767a`，本版偏差 1）、README 支持矩阵与基线名/版本行（`20e767a`/`2b54f62`）。其余笔面（新增 `CMakeLists.txt`/`cmake/`/`LICENSE`/`examples/ex_min.c`/`docs/concurrency.md`、两模板条文、ROADMAP/实机记录/申报件）属白名单或本版新建文件，不点名。
- 修订登记 @0f9389e @4b991fc @17c4df8：**v2.29 P4-4b 补登（纯登记行，`CO-6(v2.7)` 白名单式判定的登记通道；沿 v2.28 `@78aee9c @f158d05` 二次补登先例）**——本版三笔在此档前不可自列：`0f9389e` = v2.29 规划入仓批（= 快照 `snap-2.29-start` 本身，触碰 `BACKLOG.md`/`ROADMAP.md` 两非白名单件，此前从未登记）、`4b991fc` = v2.29 P4-2b/P4-3（`CHANGELOG.md` 新建 `[2.29.0]` 节 + 本档同类九档各 +1 登记行，该笔自身 SHA 无法在该笔内列出）、`17c4df8` = v2.29 P4-4（当版交付文档 `versions/v2.29/DELIVERY.md` 新建 + `README.md` 交付链接行 v2.28→v2.29 + `docs/architecture.md` 机制门行 380→382 + `docs/v3-candidates.md` 候选池与复评记录与常设条目逐版状态列）。三笔内容面逐条已入 v2.29 交付档偏差表与提交消息；载体 = `versions/v2.29/DELIVERY.md` §7.3。
- 修订登记 @04e6471 @36de066：v2.29 r2 批的非白名单触碰面（`README.md` / `.gitignore` / `docs/architecture.md`）与主档 1:1 行内修订，勘误载体 = `versions/v2.29/DELIVERY-r2.md`；本行 = 登记链维护（纯登记）
- 修订登记 @61034e1 @3820fe1 @e4a8a20：v2.29 tag 后收尾笔的统一登记（`CO-9(v2.29)-M` 处置，2026-10-21）——`61034e1` 复评记录入仓（良性面）、`3820fe1` 状态位代改（BACKLOG/ROADMAP 非白名单）、`e4a8a20` 复评勘误追加 + 提案 §8（新功能提案.md 非白名单）；tag `v2.29` 落点仍 = `810d355`
- 修订登记 @166c0d9 @f735d37 @e0d8ceb @9817b37 @d2de9e0 @e07ad51 @f912c38 @a228051：**v2.30 P3-3 登记轮（纯登记行）**——本版八笔对本文档的非白名单触碰面逐笔点名放行：`166c0d9`(入仓批: BACKLOG/ROADMAP/PLAN 三件，= 快照本身) / `f735d37`(P0: r2 档两处勘误/两模板条文/`.gitignore`/v2.29 主档补注) / `e0d8ceb`(P2: `storage/et_params.{h,c}` + `test/test_params.c` + `et_config.h`/`Makefile`/`test_main.c`) / `9817b37`(P3-2a: 版本宏 29→30) / `d2de9e0`(P2-3a: `debug/et_metrics.c` 特性表 + `test/test_metrics.c` 计数) / `e07ad51`(P1: 实机记录 §20) / `f912c38`(P2-3+P3-1+P3-2b: 文档五面 + v2.29 归档 + BASE 两处 + 数字回刷 + CHANGELOG) / `a228051`(P3-4: v2.30 交付档 + architecture 门行)；载体 = `versions/v2.30/DELIVERY.md` §3/§7.3。
- 修订登记 @4257dcd @aaa5da3 @abea3be：v2.30 r2 批的非白名单触碰面（`storage/` / `test/` / `README.md` / `ROADMAP.md` / 两 port README / `docs/` 五件 / `tools/docsync.sh` 字面 / `docs/architecture.md`）与主档纯登记笔，勘误载体 = `versions/v2.30/DELIVERY-r2.md`；本行 = 登记链维护（纯登记）
- 修订登记 @a836105：v2.30 r2 的 R6 体积行回刷笔触碰 `port/stm32f103/README.md`、`port/stm32g474/README.md`（非白名单面）与主档，勘误载体 = `versions/v2.30/DELIVERY-r2.md`；本行 = 登记链维护（纯登记）

### 7.4 ⑧ 自检表（引用机检第八周期）

| 检查 | 本文档执行 | 实测 |
|---|---|---|
| ① 判据 vs 任务自洽 | G0 窗口谓词逐文件枚举（六目录 + `et_config.h` + `Makefile` + 三门 + `apidump.sh` + `.github/` + `port/` 代码面）；`docsync.sh` 显式排除并归因（P0-4/P3-4 计划内）；**本版新形态** = AC-8 的判据前提（板侧槽容量）经实测证伪 ⇒ 按 HC-10⑤ 走 BLOCKED 而非降格凑绿（AC-15） | 本表写作时核对 ✓（冲突申报在案，见偏差 5） |
| ② 右端核对（与自称终态一致） | §4 右端 = 本提交（定稿链末笔，自指不列）；⑧ (b) 右端 = 版本键区间 `snap-2.28-start..HEAD`；AC-0 的发版锚 = tag `v2.27`（已定着，非区间端点） | 双侧证据机械可核 ✓ |
| ③ 预置数值逐项代入 | N = 375（§7.1 六步）；(d) 算式 = 33 + 1 = **34 份**（计划预置，实测以 `--emit` 机读为准）；体积增量 = +8/+12/+8 三值逐行不互推；用例增量 = 0（520 维持）；板侧 FLASH 增量 = +20 B（全量 ELF 层面，与 port 体积表 +8 B 分属两口径并各标其名） | 逐项代入成立 ✓ |
| ④ 判据串不匹配判据行自身 | 「登记行 carve-out」「结构体字段追加版的允许面」「当版占位符拦截面变化」「^  - ⑤」四处取带前缀/行首形态；引述 v2.27 的连写串时不复现原串（D-1 与 `CO-13` 教训），本档 AC-2/AC-4/AC-1 各行已按此写 | 全文自检 ✓ |
| ⑤ 文档改动登记不滞后 | 本版无对已发布交付文档的声称改写；D-1/偏差 2 属内容复原，载体 = §3 偏差表与提交消息 | ✓ |
| ⑥ 基线滚动三件事 | `docs/API_INVENTORY_v2.27.md`（worktree 于 `a02f98a` 真生成 + sha256 + 与 v2.26 归档 45 行逐段解释）/ BASE 切 v2.27（**两处实测**）/ 发版点复读 0/0/0 | ✓（AC-10） |
| ⑦ 环境注记精确 | `arm-none-eabi-size` 与 linker MEMORY USAGE 两口径并列声明；MinGW 无 libasan（`cannot find -lasan` 实测，Makefile:102-104 既有声明）；本机无 WSL/MSYS2；Python 3.12.4 + pyserial 3.5；CLI v2.19.0（CubeCLT 1.18.0）与 Device ID 0x469 | ✓ |
| ⑧ **定稿后终态复测（封闭枚举）** | **取值命令原文 = `sh tools/gatecheck.sh 2.28 --emit`**（片段由 --emit 逐字生成、粘贴禁重打）；`--verify` 对象 = 本档（本版无 rN 档 ⇒ [自指]/docref 无参对象即主档，v2.27 P0-4 条文的定位规则在 v2.28 回到经典形态）；留痕 `build/final-gates-v228.log` | 五锚行见下方片段（时点 = 定稿链末笔） |

**§7.4 行⑧ 机读值片段（`--emit` 逐字生成粘贴；时点 = 定稿链末笔）**

（下方五行为 emit 回填位，取值命令原文见上行；本档写作时不预抄任何机读值——回填由定稿批以 1:1 行内替换完成，行数与本文件行数不变）

[gatecheck:emit] (b) 全量 = 33 files / 1802 / 39
[gatecheck:emit] (b) 自排除 = 32 files / 1620 / 39 (口径 = -- ':(exclude)v2.28开发交付__*.md :(exclude)versions/v2.28/DELIVERY*.md')
[gatecheck:emit] (d) 全集 34 份 → OK 8 / 未登记实质变更 14 / 自指滞后 3 / 缺声明行 9 / 其他 0
[gatecheck:emit] (a) CHANGELOG [2.28.0] 节日期 = 2026-10-10; 全文日期计数 = 2026-09-29×5 2026-10-01×3 2026-09-25×3 2026-09-24×3 2026-09-12×3 2026-10-02×2 2026-09-30×2 2026-09-27×2 2026-09-09×2 2026-09-06×2 2026-10-10×1 2026-10-09×1 2026-10-07×1 2026-10-06×1 2026-10-05×1 2026-10-04×1 2026-10-03×1 2026-09-26×1 2026-09-19×1 2026-09-11×1 2026-09-08×1 2026-09-07×1 2026-09-05×1
[gatecheck:emit] 登记 token = v2.10开发交付__挂账清偿与板侧闭环.md=22 v2.11开发交付__勘误批与轨道收口.md=3 v2.12开发交付__勘误清偿与判据自洽.md=4 v2.18开发交付__碰撞面根治实施与记法机制化.md=4 v2.19开发交付__勘误更正与时点纪律收口.md=5 v2.20开发交付__定稿后复测机制与基线口径明示.md=58 v2.21开发交付__勘误收敛与封闭枚举.md=53 v2.22开发交付__非代码清欠与复测脚本化.md=49 v2.23开发交付__引用机检闭环与纪律块重组.md=46 v2.24开发交付__勘误收敛与verify时点固化.md=40 v2.9开发交付__模板口径澄清与板侧同步窗口-r2.md=1 v2.9开发交付__模板口径澄清与板侧同步窗口.md=54 versions/v2.25/DELIVERY.md=34 versions/v2.26/DELIVERY-r2.md=1 versions/v2.26/DELIVERY.md=29 versions/v2.27/DELIVERY-r2.md=3 versions/v2.27/DELIVERY.md=24

（以上五行由 `--emit` 逐字回填，回填笔为严格 1:1 行内替换 ⇒ 本文件行数与 (b) 双口径锚值不随之变动）

### 7.5 复现预期（tag 前后判定差）

发版（tag `v2.28`）之前：`docsync` **375/0**、`gatecheck 2.28 --verify` 对本档 = **PASS exit 0**（绿态单点 = 定稿链末笔）、`docref` 无参 = FAIL 缺声明行（本版交付文档按 HC-4 不采严格声明行形态？否——本版无 rN 档，本档即 [自指] 对象且携行首声明，`docref` 无参对象 = 本档）。发版之后：`--verify` 因 (d) 锚（本档分类与全集份数随 tag 出现而变）与 (b) 区间随后续提交增长而**转红 = 预期态**，沿 v2.26/v2.27 先例；复现差值不预判，由发版点机读承载。

### 7.6 终态门批次读数（逐件点名）

- `docsync`：**pass=375 fail=0**（终态）；`apidump --check`：exit **0**（清单 461 项与头文件一致）；`apidump --diff`（基线 v2.27）：**新增 0 / 修改删除 0**；`sizecheck`：**pass=3 fail=0**（f103 25608/24/1512、f103 selftest 38056/24/3792、g474 25984/28/1512 全 = 实测）。
- 门与工具零改动点名面：`git diff --numstat snap-2.28-start..HEAD -- tools/docref.sh tools/gatecheck.sh tools/sizecheck.sh tools/bench.c tools/pack_image.py tools/xmodem_send.py tools/modbus_master.py tools/xmodem_recv.py .github/workflows/ci.yml .github/workflows/release.yml` = **空输出**；`Makefile` 与库面五目录（`sys/ core/ algorithm/ protocol/ drivers/ storage/`）diff = **空**（库面仅 `debug/et_metrics.c` + `debug/et_metrics.h` + `test/`）。
- 用例数 `520` 终态全列点名：`grep -rn "520" README.md docs/*.md port/*/README.md tools/docsync.sh | grep -v "^docs/v3-candidates.md"` 命中行 = 文档八处 + 门两处 + 两 port host 读数行（v2.27 形态延续，本版数值未变故零改写；逐行读数随 `--emit` 留痕件承载）。
- 快照触发面实测：`git tag snap-2.28-start` 同批 `gh run list --limit 2` 最新仍 = 发版补丁 CI run（无新 run）、`gh release list --limit 1` = v2.27 Latest、`git ls-remote --tags origin refs/tags/snap-2.28-start` = 空（快照未推，无 `v` 前缀）。

## 8. CO 承接表（本版输入 → 处置 → 去向）

| # | 输入项 | 来源 | 本版处置 | 理由 / 去向 |
|---|---|---|---|---|
| 1 | `CO-7(v2.27)`-P `et_metrics_format` 槽守卫 | v2.27 r2 复评 §5 | **已交付**（P1-1 = `0415b05`，正负两组实测） | 唯一 CO-P，功能版顺车；守卫形态与同族逐字一致，公开面 0/0/0 |
| 2 | `CO-8(v2.27)` 两处口径未文档化 | v2.27 r2 复评 §5 | **已交付**（P1-2 = `1dd9db0`；计划原写 P2-4 文档批，实际落点随 P1 同批，见 §3 无偏差项——同一任务面内的顺序优化，AC-6 判据不变） | 语义零改动，纯文档化 |
| 3 | `CO-9(v2.27)`-② §10 开关枚举行无机检 | v2.27 r2 复评 §5 | **已交付**（P0-4 = `0fc6133`，K=1 + 反向探针） | 扩检查面、判定语义零变更 |
| 4 | `CO-9(v2.27)`-① [自指] fail 计数与诊断行数机制 | v2.27 r2 复评 §5 | **暂缓维持 + 入池**（P0-6 = `59ad762`） | 门判定语义变化属 v3/M2 窗口；本版实证再一次（P0-4 后 `docsync` 单断言双诊断行 → 单诊断行两态切换） |
| 5 | `CO-10(v2.27)` README 清单⑧ N 公式缺项 | v2.27 r2 复评 §5 | **已交付**（P0-5 = `e03cc50`，本版即新公式首例自证，N=375 三方一致） | 公式补 K 与 ±占位符拦截面两项 |
| 6 | `CO-11(v2.27)` 主档 AC-9 行文本形态缺陷 | v2.27 r2 复评 §5 | **已在 v2.27 侧清偿**（R9 `fc5b719`，Owner 选二择一之①） | 本版不重复排队（`versions/v2.28/PLAN.md` 变更记录行 2） |
| 7 | `CO-12(v2.27)` 模板列数/列序映射未注记 | v2.27 r2 复评 §5 | **已交付**（P0-2③ 五列对齐 = `feb3645`，`RC-1` 同题合并） | 裁决 = 模板对齐五列；本档 §3 即五列首例 |
| 8 | `CO-13(v2.27)` 评审侧判据作用域/落点枚举纪律 | v2.27 r2 复评 §5 | **未并入本版**（偏差 8：转 v2.29 判定通道 + 本版现网实证） | 追加收录需 Owner 批准本版扩容；本档已按该口径自防（AC-1 判据串取行首形态） |
| 9 | `CO-4(v2.27)` 登记行 carve-out 计划内部矛盾 | v2.27 r2 复评 §5 | **已交付**（P0-2① 条文 + 本版 HC-1 已写 carve-out） | 计划侧条文补齐 |
| 10 | `CO-5(v2.27)` HC-1 未列 API_STABILITY | v2.27 r2 复评 §5 | **已交付**（P0-2② 交叉引用条文） | 本版无字段追加，条文在位防下版复发 |
| 11 | `CO-6(v2.27)` 幻影 ` M`（autocrlf 无属性） | v2.27 r2 复评 §5 探针 | **已交付**（P0-3 `.gitattributes` = `993a05e`，Owner 裁决彻底修法；过渡条文 = P0-2⑤） | 零 blob 改写实测在案；`git config` 不由 Agent 改（Owner 约束维持） |
| 12 | `RC-1/RC-2/RC-3`（v2.27 r2 档未复评回收项） | v2.27 DELIVERY-r2 §5 | **RC-1 已交付**（五列）、**RC-2 口径已交付**（P0-2④）+ 处置「拒绝不修」明载、**RC-3 已关闭留案**（冻结件不回改） | 复评 r2 记录已入仓（`68ca5bd`），自赋号至此闭环 |
| 13 | 跨版链：`CO-8(v2.8)-P` / `CO-4(v2.8)-M` / `CO-9(v2.8)-M` / `CO-7(v2.7)-M` / ch32x035 与 f103 挂账 / v3 池 | 承接链 | **暂缓（重新声明，触发条件逐条复述不变）** | 本版窗口仅 G474；本轮枚举实测无 WCH-Link（COM16 不在位）；f103 无上板动作，Renode CI 门常态兜底 |
| 14 | REQ-6 / REQ-8 / REQ-9 状态位 | BACKLOG | **不代转**（维持「已纳入 v2.27」） | 判据前件（v2.27 评审通过 + 发版）本版已成立，但计划变更记录初行明示不代转，改判归 Owner（铁律 9） |
| 15 | 本版新发现：ASan 面的可达性缺口（本地不可跑、CI 仅 push main/PR） | 发版后补丁 P-1 + 本批实测 | **登记为遗留与候选**（§6 第二条），不自裁条文 | 候选 = 模板 §7 增「发版前 Linux 侧内存门」动作项 或 PR 触发惯例，判归 A2/Owner |
| 16 | 本版新发现：REQ-7 判据前提缺口（槽容量维度未纳入起草期核验） | AC-8 / `BLOCKED-2` | **申报 + 三条解阻路径**，建议模板 §7② 补该维度 | 同族 = `CO-13(v2.27)` 判据口径面；与 `CO-1(v2.27)` 一并交 A2 判定 |

**合规声明**：本版严格执行面 = 冻结计划 P0~P4 全项，未新增计划外任务；计划面外触碰面两处均已登记（偏差 1 = D-1 已发布档复原走 HC-4 放行；偏差 7 = v2.27 发版代做经 Owner 明示授权并落变更记录）；§1.3 全部 Non-goals 维持（零公开面新增 0/0/0、不新增开关与板上套件、门判定语义不变、不做 REQ-5 的 CI 化、不动两板工程本体、不代转状态位、v2.25~v2.27 各档内容零改写除登记行 carve-out 与偏差 1 的复原面）；**AC-8 未达成如实呈报，未以部分达成或降格判据充数**。
