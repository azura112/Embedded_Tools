# v2.30 审核记录

> 填写：审核者（阶段 4，A2 独立会话 ≠ 执行者）。权威模板 = `docs/评审记录模板.md`；流程 = `PROCESS.md`（冲突以 §15 本仓适配档案为准）。
> 结论唯一 = **有条件通过**（0 阻塞项；条件 = §6 任务清单 T-1…T-5）。另有 **2 项须 Owner 裁决**（`ET_PARAM_U32` 高半区语义、v2.29 发版顺序）不入执行者清单。

## 0. 元信息

| 字段 | 值 |
|---|---|
| 被评审版本 | v2.30（功能版 · 出口 A · 全量档；主题 = 真固件升级链闭合 REQ-7 / M4-④ + 参数版本化 REQ-12 / M5-①） |
| 交付 commit | **`27e00ef`**（定稿链末笔 = 当前 `HEAD`；tag `v2.30` 不存在） |
| 被评审文档 | `versions/v2.30/PLAN.md`（已批准-冻结，2026-10-21）、`versions/v2.30/DELIVERY.md`（§1–§8，**154 行**） |
| 输入来源声明 | `versions/v2.29/REVIEW.md`（r1 有条件通过）+ `REVIEW-r2.md`（r2 **通过**）+ `BLOCKED-2(v2.29)`；`versions/v2.30/REVIEW.md` 落笔时不存在（本记录为其产出）✓ |
| 基线（回滚点） | `snap-2.30-start` = **`166c0d9`**（入仓批末笔）；计划基线 = `tag v2.29` = **`810d355`**（本地在位、`main` 祖先；**未推送**——`gh release list` 无 v2.29、`origin/main` = `ef339dd`、本地 `main` 领先 **24** 笔，逐项实测与 AC-0 同值） |
| diff 范围 | `git diff --stat snap-2.30-start..HEAD` = **39 files / 2259 insertions(+) / 50 deletions(−)**；`git rev-list --count` = **10 笔**；冻结记录范围 `..f912c38` = **29 files / 2094 / 49**（逐值实测） |
| 评审日期 | 2026-10-22（取数时点 = `27e00ef`；落笔时点 HEAD 未变 ⇒ 双时点同 SHA） |
| 评审结论 | **有条件通过**（0 阻塞项 + 5 条任务 + 5 条 CO + 2 项 Owner 裁决） |

---

## 1. 核验方法与范围

**先 diff 后文档**：`git diff --numstat snap-2.30-start..HEAD` 逐 39 件 → 归属 HC-1 允许面 → 再读 PLAN/DELIVERY 对账。细读面 = **`storage/et_params.{h,c}` 全文（404+142 行）**、`test/test_params.c`（413 行）、`docsync.sh` 的 `et_params` 断言块（`:660-671`）、`debug/et_metrics.c` 一行 + `test/test_metrics.c` 三处计数、`移植stm32实机记录.md §20`、`build/v2.30-evidence/` 全目录、`ROADMAP.md` M4/M5 段、两模板 P0-2 条文、`versions/v2.29` 两档 1:1 修订。

**独立复现的门禁与回归**（不采信交付贴读数）：

| 件 | 本机实测 |
|---|---|
| `sh tools/docsync.sh` | **pass=395 fail=0**、stderr **0 字节**；[自指] 三方一致 = 本档行首声明 395/395 = `docs/architecture.md:114` 395 = 实测 395 |
| `sh tools/apidump.sh --check` / `--diff` | exit **0** / **新增 22 项 · 修改删除 0 项**；22 项逐条 = 9 函数（`init/load/save/validate/get/set/factory_reset/migrate/loaded_version`）+ 5 类型（`et_param_type_t`/`et_param_desc_t`/`et_params_schema_t`/`et_params_t`/`et_params_migrate_fn`）+ 8 宏（`ET_MODULE_PARAMS`、`KEY_BASE/GEN/STRIDE`、`MAX_FIELDS/SLOT_BYTES/VAL_MAX/WORK_SIZE`）——**`et_xmodem`/`et_bootctl`/`port` 前缀零条目** ✓ |
| `sh tools/sizecheck.sh` | **pass=3 fail=0**（f103 `27192/32/1520` 与 `39636/32/3808`、g474 `27860/36/1520`，与两 port README 逐值同）|
| 全量回归四形态 | 逐套件 `pass=` 求和 = **536 / 536 / 537 / 543**、fail 合计 0；`storage/params => pass=10` ✓；`make ex` **六配方 PASS** ✓ |
| `sh tools/docref.sh` | **主档 = rc=1「<END>..HEAD 存在未经修订登记的实质变更」**（`a228051`/`1653552`/`27e00ef` 三笔，结构性，交付 §4 已声明）；**v2.29 主档 = rc=0**（P0-4 补注 + 登记后转绿）、**v2.29-r2 = rc=1 缺声明行**（既定形态）、v2.28 与 v2.20–v2.24 = rc=0 ✓ |
| `(b)` 双口径 | 全量 = **39 files / 2259 / 50**；自排除 = **38 / 2106 / 50** —— 与 ⑧ 逐值同 ✓ |
| 登记 token | 本机同源管线复算 = **21 档逐值同**（v2.20=88 / v2.21=83 / v2.22=79 / v2.23=76 / v2.24=70 / v2.25=64 / v2.26=59 / v2.27=54 / v2.28=33 / **v2.29 主档=15 / v2.29-r2=4** + 余九档原值）✓ |
| ⑧ 五锚粘贴面 | 本档 ⑧ 五行 vs **`build/p35_emit2.log` 5/5 逐行逐字节同** ✓；(a) `[2.30.0] = 2026-10-22` ✓；38 份全集（我 `ls` 实数为 38）✓ |
| `--verify` | **评审侧独立取得 PASS；交付侧无留痕**：`--emit` **rc=0**（57 行）+ `--verify versions/v2.30/DELIVERY.md` = **PASS · exit 0**（两 `.err` 各 0 字节），于**固定 HEAD 的独立 worktree**（`/tmp/wt230` @`27e00ef`，运行期零提交，用毕 `remove`+`prune`）跑；五锚全类同值（含 (d) 38 份 → 10/15/3/10/0、token 21 档）。**交付物自身**：`build/final-gates-v230.log`（HC-3 指名牌）**不存在**（全盘 find = 0），`build/` 内无任何 v2.30 verify stdout/rc 件 ⇒ AC-12 的留痕义务仍缺（CO-5/T-4） |
| 契约/计数面 | MINOR **30**（`gcc -E -dM`）；开关 **33**（`grep -c '^#define ET_MODULE_'`）；模块 **36**（`architecture.md:74`「36 模块按场景查」+ docsync 并发表逐名对账动态计数）；用例 **536** 全列（README×2 / architecture / API_GUIDE×3 / getting-started×2 / 两 port host 行）——**但 port 两处仍写 526**（见 CO-3）；`API_INVENTORY_v2.29.md` = **475 项 / 806 行 / 25,233 B / sha256 `276224cd…`** 逐值同 ✓；BASE 两处（`apidump.sh:15/:227`）✓；`CHANGELOG [2.30.0] = 2026-10-22` ✓；`.gitignore:30 = .qoder-credits/` 且 `check-ignore` 命中 ✓ |
| 库面/门面 | `core/algorithm/sys/protocol/drivers` diff = **空**；`port/` 仅两 README（体积行，允许面）✓；`storage/et_bootctl.*` 与 `tools/{docref,gatecheck,sizecheck,board_window}` diff = **空**；`debug/et_metrics.c` = **1+/0−**（偏差 3）、`test/test_metrics.c` = **4+/4−**（三处计数）✓；`docsync.sh` 改动 = K=10 断言块（`:660-671` 实测 10 条 `assert_grep`）+ 2 处 `526→536` 字面 |
| 板侧（库外）复核 | `build/v2.30-evidence/`：`dual_walk_1k.log` = **`block 86/86 ACK (累计重试 0, 16.6s)`** + 判据「`staged=1 confirmed=1 attempts=1` **命中**」+ `ver=2.30.0`；回滚日志 = `staged=1 confirmed=-1 attempts=1` → 累积后清状态（`-1/-1/0`）；`img_odd/even.bin` = **87,884 B**、`fw.bin` = **87,852 B**（= body + 32 B 头，≤ 131,072）✓；`build.log` = FLASH **88,608 B / RAM 10,000 B**（与交付 AC-7 的 87,836/9,992 **不符**，见 CO-3）；`version_check.txt` 命中 2.30.0 ✓；`flash.log` 在档 |

**评审探针**（全部在 TEMP / 临时树 / 库外，主树零写入、零提交）：

1. **PP-1（`ET_PARAM_U32` 值域探针，真实代码 + 内存 kv 桩）**：编译 **未改动的 `storage/et_params.c`**（+ 64 槽 RAM kv 桩）跑 `et_params_set`：`0x7FFFFFFF → true`；**`0x80000000 → false`、`0xFFFFFFFF → false`** ⇒ `ET_PARAM_U32` 无法表达高半区（16/32 位无符号语义被 `int32_t` 中转而拒绝）。根因 = `et_params.c:44-54 load_i32` 返 `int32_t` + `:105` `(type != I32) && (v < 0) → false`；头注与用例均未声明该上限（`test_params.c` 只用 baud ≤ 921600）。**见 CO-1（CO-P）**。
2. **PP-2（docsync 断言删除反向探针）**：在 `git archive HEAD` 临时树删 `et_params.h` 的 `ET_PARAMS_KEY_GEN` 断言行 → `docsync: pass=393 fail=1`，两条 [自指] 红（`architecture 395 ≠ 实测 394`、本档声明 395 ≠ 394）⇒ K 块与三方一致对账为**真判定**（删除即被计数耦合捕获）。用毕删除临时树。
3. **PP-3（`load_gen` 回退路径的静态复核，无探针）**：`et_params.c:223` 回退另一代际时**复用 GEN 的 `ver` 作校验值**（`load_gen(p, 1-g, ver, …)`），而另一代际的 `KEY_VER` 是其自身旧版本 ⇒ **当两代际 schema 版本不同（= 发生过升级）时该回退必然失败**，落回默认值而非旧配置；头注 `et_params.h:13` 的「当前代际损坏 ⇒ 回退另一代际」未加版本限定。见 **CO-2（低）**（静态判读，未构造损坏镜像探针）。

**未核验项（不得默认为通过）**：

- **`--verify`（评审收笔后由固定 HEAD 的独立 worktree 补跑，已闭合）**：`/tmp/wt230` @`27e00ef`（detached，运行期零提交）—— `--emit` **rc=0**（57 行；`build/a230-wt-emit.err` 0 字节）、`--verify versions/v2.30/DELIVERY.md` = **PASS · exit 0**（原文「与本轮机读值逐行一致 (⑧ 引用机检绿)」，`build/a230-wt-verify.err` 0 字节）。五锚**全类**与 ⑧ 逐值同——(b) 39/2259/50 与 38/2106/50、(d) **38 份 → OK 10 / 未登记 15 / 自指滞后 3 / 缺声明行 10 / 其他 0**、(a) 2026-10-22、token 21 档。**AC-12 据此改判「通过」**；但**交付物自身**仍无该 stdout/rc 与 HC-3 留痕件（CO-5/T-4 维持）。耗时实测 = emit **51 分钟** + verify **46 分钟**（同机 38 份 docref，工作树位于 C: 临时区），远长于 v2.29 的 25+2 分钟 ⇒ 长跑门时长不可外推。
- **ASan 双变体 / ARM 交叉 / Renode**：本轮未跑（时间预算优先给新模块与板侧证据面）；证据 = 交付贴读数与 CI 历史，未独立复跑。
- **板侧私有件逐行（`et_demo.c` / 板侧 `CMakeLists.txt`）**：以证据目录原文 + `sync_report.txt`（3 件残差 0）+ 实机记录 §20 等效复核，未逐行核私有源。
- **115200 + 1K 块型的复测**：交付 §3 偏差 5 已如实声明「修正纪律后未复测」✓（不判缺陷，属登记项）。
- **本版发版步**（v2.29 push/Release、v2.30 merge/tag/CI）：属 Owner（HC-0②），未代做、未预写其绿。

**条文级核查面**：docref 判读先看失败阶段 ✓（本档 = 反滞后「<END>..HEAD」，与「缺声明行」不同桶）；碰撞规避 ✓（散文行不携 token）；按值声明 + 机械抽检 ✓（三处失准见 CO-3）；⑧ 封闭枚举四类 ✓（(a)/(b)/(token) 独立复算同值、(d) 行未独立复算但份数与桶和同源）；verify 绿态 = 定稿链末笔 ✓（末笔 = HEAD，无后交付笔 ⇒ 补跑条件成立）。

---

## 2. 硬性约束（HC-n）逐条核验

| HC | 结论 | 证据（本机独立复现） |
|---|---|---|
| **HC-0** 基线异常与推送顺序 | **通过（异常已如实登记）** | `tag v2.29 = 810d355` 本地在位且为 `main` 祖先 ✓；`gh release list` **无 v2.29**、`origin/main = ef339dd`、本地 `main` 领先 **24** 笔（交付写 24，实测 24）✓；推送顺序「v2.29 先于 v2.30」在 §1 首注与 §6 在案 ✓；**未拿本地 tag 冒充已发布** ✓ |
| **HC-1** 范围封闭 | **部分达成（两件面外，偏差 3）** | 新增三件（`et_params.{h,c}` / `test_params.c`）✓；`et_config.h`（版本 + 开关）、`Makefile`（两列表）、`test_main.c`（注册 2 行）✓；**`debug/et_metrics.c` 1 行 + `test/test_metrics.c` 4/4 不在允许面**（偏差 3 已登记，成因 = 特性表计数门 vs `g_features` 必须新增行；处置取「不留红门」侧，我核其内容 = `ET_FEAT(PARAMS)` + `33/34` 与 `sw=33` 三处，零判定语义变化）✓；六库目录其余 diff = 空 ✓ |
| **HC-2** git 锚定与触发面 | **通过** | `snap-2.30-start = 166c0d9` 无 `v` 前缀 ✓；`gh run list --limit 3` 最近仍为 v2.28-r2 R15（**无 2.30 新 run**）、`gh release list` 无 2.30 ✓ |
| **HC-3** 证据面 | **部分达成** | ⑧ = 逐字粘贴（5/5 行与 `p35_emit2.log` 逐字节同）✓；**对照面** = 改前/改后三组停止点与 86/86 零重试读数在 §20 ✓；**留痕件 `build/final-gates-v230.log` 不存在**（全盘 find = 0；`build/` 无 verify 件）⇒「段数 = 声称面」义务无载体（连续第二版）→ **CO-5 / T-4** |
| **HC-4** 修订登记放行 | **通过** | `versions/v2.29/DELIVERY.md` = 3+/1−（AC-8 补注 1:1 + 短语行 + token 行）、`DELIVERY-r2.md` = 5+/3−（3 处 1:1 + 短语 + token）✓；两档 docref 均 rc=0 / 缺声明行（各自既定形态）✓；旧串「R4 = 定稿链末笔」与「R4 = 末笔」**0 命中**、新形态「记录范围 END = R4；末笔 = R5」3 命中、「行首具名 **3**」1 命中 ✓ |
| **HC-5** 数字回刷同批 | **部分达成** | MINOR 30 / `0x021E00` ✓；README 版本行 + 交付链接行 + 基线名 ×2（→ v2.29 归档）✓；模块 **36** / 开关 **33** ✓；用例 **536** 四形态实测同值 ✓；CHANGELOG 日期 ✓；体积三行 = sizecheck 3/0 ✓。**残留 = 两 port host 读数行仍写 526**（`port/stm32f103/README.md:175`、`port/stm32g474/README.md:122`）⇒ 交付 AC-10「`grep -rn 526` 终态 0 命中」**不成立**（另两处命中为 `405265e` 哈希与 CHANGELOG 的「526→536」历史句，非残留）→ CO-3 / T-3 |
| **HC-6** 契约零破坏 | **通过** | `--check` rc=0；`--diff` = 新增 22 / 删改 0，点名清单全为 `et_params` 面（9 函数 + 5 类型 + 8 宏）✓；`et_xmodem`/`et_bootctl`/`port` 零条目 ✓；`API_STABILITY` 本版无 §5.1 追加行（`grep -c v2.30` = 1，为「本版无追加」说明）✓ |
| **HC-7** 门改动边界 | **通过** | `docsync.sh` = K=10 纯新增断言块（我逐条核对 10 条）+ 2 处 `526→536` 字面，判定语义零变更 ✓；其余四门 diff = 空 / `apidump` 仅 BASE 两处 ✓；PP-2 反证断言块被计数耦合守护 ✓；N 复算 384+10+1+1−1 = **395** 与实测/门行/声明三方一致 ✓ |
| **HC-8** 如实性与口径 | **部分达成** | BOOTINFO 归因重建 ✓（confirm 从 `-1/-1/0` 基线起算到 `1/1/1`，证据原文在档）；断电注入 100（`params_powercut_100`，`test_params.c:323`）✓；`--ver` 奇偶标注 ✓；偏差 5（停止点归因限定）如实 ✓。**三处按值失准** = AC-7 的 FLASH/RAM、AC-10 的 526「0 命中」、AC-1 的 r2 档「139→140」（终态 **141**）→ CO-3 |
| **HC-9** 分层与不变式 | **通过** | `et_params.h` 仅含 `stdint/stdbool/stddef/et_config/et_kv` ✓（不依赖 `sys/`/`debug/`）；`et_params.c` 仅 `string.h`；**零 malloc/calloc/free** ✓；句柄化（调用方工作区）✓；全 API 标 `🏠MAIN` + 头注写明 flash 擦写不可 ISR ✓ |
| **HC-10** 板侧义务 | **通过** | ① 修复顺序 = 1K → 静默 → 波特率（证据三组 attempt 日志 + §20 对照）✓；② 双单判据 = `BOOTINFO` 三字段自本次 stage 起算 + `AT+VER` 2.30.0；回滚 = A 槽 `SELFTEST 22/22` + attempts 累积后清状态 ✓；③ 烧前版本串机检 ✓；④ 证据落 `build/v2.30-evidence/` ✓；⑤ 未走 BLOCKED（传输面已闭合）✓；**115200+1K 未复测**如实登记（偏差 5）✓ |

---

## 3. 验收标准（AC-n）逐条核验

| AC | 交付声称 | 核验结果 | 本机证据 |
|---|---|---|---|
| **AC-0** | 通过（异常已登记） | **通过** | 见 HC-0 全行（tag/远端/gh 三项逐值同）|
| **AC-1** | 通过 | **通过（附一处时点数字）** | r2 档 3 处 1:1 新形态 ✓、旧串 0 ✓、docref = 缺声明行 ✓；**「139→140」为 P0 中间态，终态 = 141**（P3-3 补 token 行 +1）→ CO-3 |
| **AC-2** | 通过 | **部分达成（M4 状态行矛盾）** | 两模板条文各 1 ✓；`.qoder-credits` 忽略 ✓；v2.29 AC-8 补注在位 ✓；**`ROADMAP.md:34` M4 达成状态仍写「④ 部分——真固件双单传输面受阻（`BLOCKED-2`）」**，而 M4 已移入「已结案阶段」（`:25/:27`）且交付 AC-14/§8 称「M4 结案」⇒ 结案与状态行自相矛盾 → CO-4 / T-2 |
| **AC-3** | 通过 | **通过（附 CO-1 语义缺口）** | `et_params.h` 公开面在 `--diff` 点名清单内 ✓；代际原子逻辑我逐行审：写非活跃代际 → `et_kv_commit`（压实）→ **最后**写 GEN（`et_kv_set` 即时持久化，`et_kv.h:12`）⇒ 提交点语义成立 ✓；`params.gen_switch`/`powercut_100` 用例在位 ✓；**U32 高半区拒绝** → CO-1（PP-1 实证）|
| **AC-4** | 通过 | **通过** | `make WERROR=1 test` **536/536**、`storage/params pass=10` ✓；100 点注入用例在位（`test_params.c:323`，注入点 `w0 + span*i/100`，i=0…99）✓；迁移失败回滚用例在位 ✓ |
| **AC-5** | 通过 | **通过** | 五面机读命中：docsync 断言块 **10 条**（`et_config`/`Makefile`×2/`test_main`/`README`/`API_GUIDE`×2/`et_params.h`×2/`et_params.c`）✓；模块 36 / 开关 33 ✓ |
| **AC-6** | 通过 | **通过** | `--check` 0、`--diff` 22/0；前缀面零变更；`API_STABILITY` 无追加行 ✓ |
| **AC-7** | 通过 | **部分达成（按值失准）** | `ET_XM_1K` 生效（86 块 × 1K，86/86 ACK 0 重试 16.58 s）✓；升级期 TX 静默（证据 0 重试 + 判据命中）✓；**`FLASH=87836 RAM=9992` 与证据 `build.log` = 88,608 / 10,000 不符**（`window_run.log` 同 88,608/10,000）→ CO-3 |
| **AC-8** | 通过 | **通过** | confirm（`-1/-1/0` → `1/1/1` + `ver=2.30.0`）✓；rollback（`staged=1 confirmed=-1 attempts=1` → 清状态 + A 槽 `SELFTEST 22/22`）✓；镜像 87,884 = body 87,852 + 32 B ≤ 131,072 ✓；`--ver` 奇偶（7 奇/8 偶）逐处 ✓；归因纠正（P0-4）✓；**未以小镜像冒充** ✓ |
| **AC-9** | 通过 | **通过** | `API_INVENTORY_v2.29.md` = 475 项 / 806 行 / 25,233 B / sha256 `276224cd…` 逐值同 ✓；BASE 两处 ✓；复读 22/0 ✓ |
| **AC-10** | 通过 | **部分达成（526 残留）** | MINOR/模块/开关/用例/CHANGELOG/体积行均实测同值 ✓；**`grep -rn 526` = 0 命中不成立**（两 port host 行仍 526）→ CO-3 |
| **AC-11** | 通过 | **通过** | 四形态 536/536/537/543 ✓；`make ex` 六 ✓；docsync 395/0 ✓；apidump 0 ✓；sizecheck 3/0 ✓；CI/Release 归 Owner（未预写）✓；快照无新 run ✓ |
| **AC-12** | 见 §7.4⑧ 与 §7.5 | **通过（评审侧独立取得；交付物留痕缺）** | ⑧ 与 `p35_emit2.log` **5/5 逐字节同** ✓；评审侧固定 HEAD 独立跑（worktree @`27e00ef`，已清理）：`--emit` **rc=0**、`--verify` **PASS exit 0**（stderr 各 0 字节），五锚全类同值（含 (d) 38 份 → 10/15/3/10/0、token 21 档）✓；**交付物自身无 verify 留痕与 `final-gates-v230.log`** → CO-5 / T-4 |
| **AC-13** | 通过 | **通过** | G0 谓词面（`docref/gatecheck/sizecheck/apidump` + 六库目录 + `port/` + `Makefile` + `et_config.h` + `.github/`）在 P0 时点 diff 空 ✓；`docsync` 计划内 K=10 ✓ |
| **AC-14** | 通过 | **部分达成（M4 结案面矛盾）** | v3-candidates「v2.30 复评记录」= 1 节、数据行 **18**（15 CO 输入 + 3 常设）✓；三板 v2.30 行在位 ✓；池 F/G/H 三条在位 ✓；**M4 结案与 `ROADMAP:34` 矛盾** → CO-4 |

### 3.1 AC ↔ 冻结 API 形状互洽（复核项）

- `et_params` 为**纯新增**（22 项），既有签名零删改 ✓；`et_kv` 签名未动 ✓（仅依赖其 `get/set/commit` 三个既有函数）。
- 代际键空间：`KEY_FIELD = 0x6000 + g*0x100 + i`（`i < MAX_FIELDS = 32`）⇒ 0x6000–0x601F / 0x6100–0x611F；`KEY_VER = 0x6200 + g`；`KEY_GEN = 0x7FF0` —— 与 `ET_KV_KEY_MAX = 0x7FFE` 无越界、与字段区间无碰撞 ✓（逐宏实算）。
- `ET_PARAMS_VAL_MAX = 30`、槽 32 B、工作区 `count * 32`：`slot_set_default` / `load_gen` 的 `tmp[30]` 边界自洽 ✓。
- **值域语义**：`min_i/max_i` 为 `int32_t`，`U8/U16/U32/I32` 共用 `load_i32` ⇒ `U32` 高半区不可表达（PP-1 实证）；`U8/U16` 无此问题（≤ 65535）；BLOB 走纯长度校验 ✓。见 CO-1。

### 3.2 基线滚动复核项

- 三件事显式执行 ✓：v2.29 归档 475 项 / BASE 两处 / 复读 22+0 ✓；旧归档零触碰 ✓；README 基线名两行 → `API_INVENTORY_v2.29.md` ✓。

---

## 4. 偏差记录核验（交付 §3 六项裁决）

| # | 偏差 | 裁决 | 理由 |
|---|---|---|---|
| 1 | P0 提交消息合计失准（10+/5− vs 实测 10+/4−） | **接受** | 我实测 `f735d37` = **10+/4−**，逐件 5 件；不可回写处置正确；「命令渲染模板」后续笔（`msg_*.txt` 8 件在位）为有效止损 ✓ |
| 2 | P2 提交消息合计与逐件均失准（776+/5− vs 实测 966+/2−） | **接受** | 我实测 `e0d8ceb` = **966+/2−**，逐件 = Makefile 2/2、`et_config.h` 3/0、`et_params.c` 404/0、`et_params.h` 142/0、`test_main.c` 2/0、`test_params.c` 413/0 —— 与交付自报「404/142/413」**逐值同** ✓；同一错误第三次，入池 C 合适 |
| 3 | `debug/et_metrics.c` + `test/test_metrics.c` 被触碰 | **接受** | 计划内在互斥成立（新开关必入 `g_features` 且「表行数 == 开关数」有断言；三处计数 32→33/33→34 必须随动）；我核内容 = 1 行 `ET_FEAT(PARAMS)` + 4/4 计数，零语义 ✓；池 F 条文建议正确 |
| 4 | 板侧附加两改（57600 兜底 / RSTCSR 诊断） | **接受** | 证据含 `BAUD_UPG = 57600` 与 `RSTCSR=14000000` 原文；均库外、不改库面协议 ✓ |
| 5 | 传输停止点归因限定（中间三轮混入探针纪律缺陷；115200+1K 未复测） | **接受（符合 HC-8③）** | 如实登记，不夸大因果 ✓；可用形态 1K+静默+57600 有 86/86 证据 ✓；池 G/H 覆盖剩余面 ✓ |
| 6 | P0-1/P0-4 的 token 延后到 P3-3 | **接受** | 自指不可预写（RC-5）；短语行与 token 行分行、P3-3 补列后在档（我核两档 token 命中） ✓ |

**second-pass 新发现（交付未登记）**：① `ET_PARAM_U32` 高半区拒绝（CO-1）；② `load_gen` 跨版本回退失效（CO-2）；③ 三处按值失准（CO-3）；④ M4 结案与状态行矛盾（CO-4）；⑤ HC-3 留痕件缺失 + verify 无留痕（CO-5）。

---

## 5. 遗留问题清单（本记录对下一版的输入）

| 编号 | 问题 | 证据 | 分级 / 建议处置 |
|---|---|---|---|
| **CO-1(v2.30)-P（中）** | **`ET_PARAM_U32` 无法表达高半区（≥ 0x80000000）**：`load_i32` 返 `int32_t`，`field_value_ok` 对非 I32 类型拒绝负值 ⇒ U32 的 `0x80000000…0xFFFFFFFF` 全部 set/validate/load 失败；头注与用例均未声明该上限（用例只覆盖 baud ≤ 921600） | **PP-1 真实代码探针**：`set(0x7FFFFFFF)=true`、`set(0x80000000)=false`、`set(0xFFFFFFFF)=false`；根因行 = `et_params.c:44-54` 与 `:105`；`et_params.h:68-69` 未写上限 | **CO-P（中）**（新 API 首版的值域缺陷，影响「U32 计数器/ID/掩码」类参数）。处置 = **T-1**，二选一由 Owner 裁：(a) **修**——`field_value_ok` 对 `ET_PARAM_U32` 走无符号比较（`uint32_t` 中转，`min_i/max_i` ≥ 0 时按无符号解释）+ 加 3 条边界用例（0x7FFFFFFF / 0x80000000 / 0xFFFFFFFF）；(b) **不改语义**——头注 + API_GUIDE §6.3 明写「U32 上限 = 0x7FFFFFFF（`min_i/max_i` 为 int32 表示）」并加一条**断言该文档化行为**的用例。两路均不破坏「纯追加」契约（(a) 属行为修正，需在交付说明区登记） |
| **CO-2(v2.30)-M（低-中）** | **`load_gen` 的另一代际回退在跨版本时不可用**：`et_params.c:223` 以 **GEN 的 ver** 校验另一代际，而另一代际 `KEY_VER` 是其自身旧版本 ⇒ 当前代际损坏且版本已升级时，回退必然失败 → 落默认值；头注 `et_params.h:13`「当前代际损坏 ⇒ 回退另一代际」未限版本 | 静态逐行复核（PP-3，未构造损坏镜像探针）；`load_gen` 的 `ver != gen_ver` 拒绝路径 `:132-134` | **CO-M（低-中）**（注释/文档承诺 vs 实现边界）。处置 = **T-1 同批**：或改为读取另一代际自身的 `KEY_VER`（不传 GEN 的 ver），或在头注与 API_GUIDE 明写「回退仅限同版本代际，跨版本损坏落默认值」+ 用例覆盖（后者零代码风险） |
| **CO-3(v2.30)-M（低-中）** | **按值失准三处**：① AC-7 与实机记录 §20 写 `FLASH=87836 RAM=9992`，证据 `build.log`/`window_run.log` = **88,608 / 10,000**（偏差 4 的两处诊断改动在后，数字取自改前构建）；② AC-10「`grep -rn 526` 终态 **0 命中**」不成立——`port/stm32f103/README.md:175` 与 `port/stm32g474/README.md:122` 仍写 **526**（另两处命中为哈希 `405265e` 与 CHANGELOG 的「526→536」历史句，非残留）；③ AC-1「r2 档 139→**140**」为 P0 中间态，终态 = **141**（P3-3 token 行 +1） | ① `grep -rh FLASH build/v2.30-evidence/build.log` = 88,608；`window_run.log:24` = 88,608/10000；② 全仓 `grep -rn 526 --include=*.md`；③ `git diff --numstat snap..HEAD -- versions/v2.29/DELIVERY-r2.md` = **5+/3−**、`wc -l` = **141** | **CO-M（低-中）** = `CO-7(v2.26)` 族**第九/十/十一例**。处置 = **T-3**（三处 1:1 行内勘误 + 两 port host 行 526→536）；机制面：**「grep 0 命中」类声称必须回贴命令输出**（池 C 同族） |
| **CO-4(v2.30)-M（低-中）** | **M4 结案与自身状态行矛盾**：M4 已移入 `## 已结案阶段（归档）`（`ROADMAP.md:25/:27`），交付 AC-2/§8 称「M4 结案」；但 M4 的**达成状态行（`:34`，2026-10-21 r2 后）仍写「④ 部分——真固件双单传输面受阻（`BLOCKED-2`）」**，未反映本版 P1 达成 | `sed -n '34p' ROADMAP.md` ④ 段原文；`grep -n '^\\*\\*M4'` = `:27` 位于已结案段 | **CO-M（低-中）**（ROADMAP 状态准确性；v2.28 偏差 6 同族）。处置 = **T-2**：把 `:34` 的 ④ 改为「**达成**（v2.30 P1 双单，实机记录 §20）+ 残留 = 115200+1K 未复测（池 H）」，并把该行日期更新为 2026-10-22；判据 = ④ 行含「达成」且不再引用 `BLOCKED-2` 为受阻态 |
| **CO-5(v2.30)-M（中）** | **交付侧证据面缺失**：HC-3 指名的 `build/final-gates-v230.log` 不存在（全盘 find = 0），且交付物无 `--verify` 的 stdout/rc 落盘件 ⇒ AC-12 的「verify PASS」在**交付物内**只有声称（连续第二版同型：v2.29 为 CO-4/CO-5）。**评审侧已于收笔后独立取得 PASS**（固定 HEAD worktree，见 §1/§7）⇒ 不影响「事实成立」，但**留痕义务仍缺** | `ls build/final-gates-v230.log` = No such file；`find /d/code -maxdepth 6 -name "final-gates-v230*"` = 0；评审侧 `build/a230-wt-emit.log` / `a230-wt-verify.log`（库外临时留痕，**不可替代**交付侧留痕） | **CO-M（中）**（证据面纪律 + 上版已立「段数 = 声称面」仍复发）。处置 = **T-4**：生成该件六段（docsync / apidump×2 / sizecheck / 四形态+ex / docref 逐档 / **`--emit` 五锚 + `--verify` stdout 与 exit 码**），verify 段必须在**末笔**（= `27e00ef`，其后不再入库任何笔）取；交付 AC-12 行尾加指针 |
| 跨版承接（不得静默） | `CO-12(v2.28)` / `CO-9①(v2.27)` 门语义类维持 v3 池；`CO-6(v2.29)-P` = **已达成**（P1 双单，M4-④ 收口）；`CO-7/CO-8(v2.29)` = 已修（P0-1，我复测新形态 3/0）；`CO-9(v2.29)` = 条文已入两模板（P0-2，各 1 命中）；`.qoder-credits` = 已忽略（P0-3） | 交付 §8 逐条 + 我抽测 6 项 | 维持 / 已关闭；REQ-11/13/14/15/16/17 暂缓 ✔ |

---

## 6. 面向 B2 的任务清单（可勾选；B 的输入只有本清单）

> 前置事实：**Owner 须先裁一项**——CO-1 的处置路（(a) 修无符号比较 / (b) 文档化 + 断言用例）；该项决定 **T-1** 的形态。T-2…T-5 与之无关，可即刻执行。

- [ ] **T-1 `et_params` 值域与回退语义（CO-1 / CO-2）**：按 Owner 裁决落 (a) 或 (b)；(a) = `field_value_ok` 对 `ET_PARAM_U32` 用无符号比较 + 3 条边界用例（0x7FFFFFFF / 0x80000000 / 0xFFFFFFFF），(b) = 头注 + `API_GUIDE §6.3` 明写上界 + 一条断言文档行为的用例。**同批**处理 CO-2：或让回退读取另一代际自身 `VER`，或明写「回退仅限同版本代际」+ 用例。**判据** = 三值探针（0x7FFFFFFF/0x80000000/0xFFFFFFFF）行为与文档逐值一致；四形态全绿；若 (a) 则 `--diff` 仍 22/0（签名未变）。
- [ ] **T-2 ROADMAP M4 状态行（CO-4）**：`ROADMAP.md:34` 的 ④ 改为「**达成**（v2.30 P1 双单，实机记录 §20）+ 残留 = 115200+1K 未复测（池 H）」，日期 → 2026-10-22。**判据** = `grep -c "④"` 行内含「达成」且不再有「传输面受阻」；M4 结案段与之一致。
- [ ] **T-3 三处按值勘误 + 526 回刷（CO-3）**：① 交付 AC-7 与实机记录 §20 的 `FLASH=87836 RAM=9992` → 证据值 **88,608 / 10,000**（并把「取数时点 = 含两处诊断改动的构建」写明）；② `port/stm32f103/README.md:175` 与 `port/stm32g474/README.md:122` 的 **526 → 536**（host 读数行）；③ AC-1 的「139→140」→「139→**141**（P0 +1 短语行、P3-3 +1 token 行）」。**判据** = `grep -rn 526 --include=*.md` 仅剩哈希与历史句（0 处用例计数）；三处 1:1 行内（交付/实机记录）+ 两 port 行 1:1；若改动本档行数则须重 emit。
- [ ] **T-4 留痕件 + verify（CO-5）**：`build/final-gates-v230.log`（库外、零入库）六段 = docsync / apidump `--check`+`--diff` / sizecheck / 四形态 + `ex` / docref 逐档（含本档红原文）/ **`--emit` 五锚 + `--verify` stdout 与 exit 码**；verify 时点 = `27e00ef`（末笔；若 T-1…T-3 产生新笔，则在新末笔重跑并把时点写进件内）。**判据** = 文件存在、六段标题齐、verify 段 = **PASS exit 0**（stderr 0 字节）；交付 AC-12 行尾加指针。
- [ ] **T-5 机制面（非阻断）**：把「grep 0 命中类声称必须回贴命令输出」与「HC-3 留痕件存在性」写入 `docs/评审记录模板.md`（各 1 行，纯追加）；与池 C/F 条文合并同批。**判据** = 两处各 `grep -c` = 1，旧行前缀未改。

> **执行顺序建议**：T-2/T-3 为 1:1 行内（净行数 0 ⇒ `(b)` 锚不漂）；T-1 若走 (a) 会改 `et_params.c`（库面，`(b)` +N）⇒ 该笔后必须重跑 `--emit` 并重贴 ⑧（`CO-12(v2.28)` 等价式）；T-4 的 verify 必须在最终末笔且其后零入库。

---

## 7. 风险复评（交付 §6 逐条）

| 交付 §6 风险 | 复评 |
|---|---|
| v2.29 Release 未发；推送顺序 v2.29 先于 v2.30 | **成立 ✓**——三项读数我独立复核（gh release 无 v2.29 / origin/main `ef339dd` / 本地领先 24）；顺序约束已写死，发版归 Owner |
| 传输可用形态 = 1K + 静默 + 57600；115200+1K 未复测 | **成立 ✓**——偏差 5 如实；可用形态证据 86/86 零重试；池 H 承接剩余面 |
| 接收端会话驻留 60 s（连续升级须等退出） | **成立 ✓**——属驱动纪律，池 G 在档 |
| 当版交付档 docref 结构性未登记（池 D）与提交消息合计（池 C） | **成立 ✓**——我实测本档 docref = 反滞后红（结构性）；msg 渲染件 8 件在位为有效止损 |
| 板侧两处附加改动超计划三项 | **成立 ✓**——偏差 4 登记 + 证据原文（57600/RSTCSR）在档 |
| **本记录新增**：新模块值域/回退语义（CO-1/CO-2）与 M4 状态行（CO-4） | 见 §5；均不阻断 M4-④ 的**行为面**证据（双单已实测），属 API 语义与文档一致性面 |

---

## 8. 结论与放行

**结论 = 有条件通过（0 阻塞项）。**

- **HC**：HC-0/2/4/6/7/9/10 通过；**HC-1 部分**（两件面外，偏差 3 已接受）；**HC-3 部分**（留痕件缺失 → CO-5）；**HC-5 部分**（526 残留 → CO-3）；**HC-8 部分**（三处按值失准 → CO-3）。
- **AC**：AC-0/3/4/5/6/8/9/11/12/13 通过（AC-12 由评审侧固定 HEAD 独立跑取得 PASS）；AC-1/AC-7/AC-10 部分（按值失准）；AC-2/AC-14 部分（M4 状态行矛盾）；**交付物自身的 verify 留痕仍缺** → CO-5 / T-4。
- **产品面**：v2.30 的**行为面主线达成**——真固件 1K 传输 **86/86 ACK 0 重试 16.58 s** + confirm（`1/1/1` + `ver=2.30.0`）与回滚（`staged=1 confirmed=-1 attempts=1` → 清状态 + A 槽 `22/22`）双单；新模块 `et_params` 的**代际原子语义经逐行审读成立**（`et_kv_set` 即时持久化 + 末写 GEN 提交点），10 例（含 100 点掉电注入）全绿；公开面纯追加 22 项、既有签名零改动。**唯一产品级缺口 = CO-1（U32 高半区）**，属值域语义，不阻断 M4-④ 的验收事实。
- **豁免项与依据**：无（未使用豁免）。

**放行前置（Owner 侧）**：① **CO-1 的处置路裁决**（(a) 修 / (b) 文档化 + 用例），它决定 T-1 形态；② **v2.29 的发版顺序**（先 push main + tag v2.29，再处理 v2.30 的合入；本版未代做）；③ tag `v2.30` 的落点 = **T-1…T-4 完成后的定稿链末笔**——若 T-1 走 (a)（改库面）必重跑 `--emit`/`--verify`；若仅 T-2/T-3 的 1:1 编辑，`(b)` 锚不漂但仍须在末笔取 verify 并留痕（CO-5）。

**方法注记（评审侧自暴露，不列 CO）**：

- **「声称 verify PASS 但无留痕」必须判未核验**：本轮沿用 v2.28 起的判定口径（AC-12 不因 ⑧ 面可信而默认通过）——这也是 CO-5 的直接依据。
- **长跑门用固定 HEAD 的 worktree**：本轮 `--emit/--verify` 在 `git worktree @27e00ef` 内跑（不从主树跑），以规避「运行期落新笔」的混合时点问题（v2.29 复评的教训）；证据已复制到 `build/a230-wt-*.log`（库外临时区随 worktree 一并清理）。
- **grep 类声称的 scope**：我复测「526」时把 `--include=*.md` 全仓跑了一遍并分类（残留/哈希/历史句），否则会误判为 3 处残留——该分类过程即 CO-3② 的证据。
- 编号碰面规避：本记录 `CO-3` 与交付偏差 3 不是一回事（后者是已登记的 `et_metrics` 触碰，前者是**未登记**的按值失准面），引用须带前缀。

**排除项（提出但经核验不成立，不虚报）**：

| 怀疑 | 核验结果 |
|---|---|
| `et_params` 的 GEN 提交点写在 `et_kv_commit()` 之后 ⇒ 掉电丢失提交 | **不成立**：`et_kv.h:12` 明写「任何操作即时持久化，无丢数据窗口；commit() 为手动压实」⇒ GEN 写入即持久化，提交点语义成立 |
| 新模块依赖上层（`sys/`/`debug/`） | **不成立**：头文件仅 `stdint/stdbool/stddef/et_config/et_kv`；`.c` 仅 `string.h` |
| `apidump` 22 项含既有前缀的删改 | **不成立**：22 项逐条均为 `et_params_*`，`--diff` 修改删除 = 0 |
| 板侧双单是「合成镜像」或旧读数 | **不成立**：`fw.bin` 87,852 = 当版（`ver=2.30.0`），`img_odd/even` 87,884 = +32 B 头；`dual_walk_1k.log` 为串口原文（含块级 ACK 与 BOOTINFO 原文），回滚为 even-ver 8 触发自检失败路径 |
| `docsync` 的 395 是靠改断言数放水 | **不成立**：N 推导 384+10+1+1−1 = 395 逐项成立；K 块 10 条断言我逐条核；PP-2 删除一条即触发 [自指] 红 |
| CHANGELOG 日期与终态提交日不符 | **不成立**：`[2.30.0] = 2026-10-22` = `27e00ef` 提交日 ✓ |
| `v2.29` 两档的 1:1 勘误未命中旧串 | **不成立**：旧串 0 命中、新形态 3 命中、`wc -l` 141（终态）——旧串判据自身不匹配 ✓ |

---

**评审留痕（本机 `build/` 与 TEMP，gitignore 面，不入仓）**：独立门 = `build/a230-wt-emit.log`（rc=0，57 行）+ `build/a230-wt-verify.log`（PASS rc=0）+ 两 `.err` 各 0 字节（固定 HEAD `27e00ef`；emit 51 min / verify 46 min，实测）· `a230-ds.err`（stderr 0 字节）· `a230-regression.log`（536/5
**评审全程未向仓库产生任何提交**（铁律 5）；`git status --porcelain` = `?? .qoder-credits/`（会话工具目录，非本评审产物）；对已跟踪文件的净改动 = **零**。
