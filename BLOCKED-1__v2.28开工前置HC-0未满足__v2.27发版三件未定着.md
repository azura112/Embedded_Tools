# BLOCKED-1：v2.28 开工前置 HC-0 未满足——v2.27 发版三件全部未定着

> 触发计划：`versions/v2.28/PLAN.md`（状态 = 已批准-冻结，出口 A 功能版·全量档）· 提出者：执行者（B1，`prompts/B1.md`）· 状态：**已解除**（2026-10-10，代做执行 = 执行者 B1，授权与读数见 §4；§1–§3 原样保留不抹除）
> 依据：本版计划 **HC-0**（开工前置，Owner 2026-10-09 裁决承载）与 §6「顺序规则 ① HC-0 前置」、AC-0；`PROCESS.md` 铁律 7（阻断停手，不硬扛不绕道）；`prompts/B1.md`「发现阻断性缺陷：不要动手」。
> 性质：**不是计划缺陷，而是前置动作缺口**——HC-0 三项均由 Owner/B 侧执行（计划原文：「push/tag/Release 不由本 Agent 代做」），故执行者不自行开工、不改用 SHA 锚点绕道（HC-0 明文禁止），本件即停机申报。

## 1. 现象（计划声明 vs 仓库事实，逐项对照 + 证据命令）

| # | 计划声明（HC-0 / §6 / AC-0） | 仓库事实（2026-10-09 实测） | 证据命令与输出 |
|---|---|---|---|
| 1 | HC-0 ②：`git tag v2.27` 必须落在主批定稿链末笔 **`a02f98a`**（= v2.27 ⑧ 片段与 `--verify` 的定义时点；v2.27 评审 §8「放行前置三条」第 1 条原文；**不是 `db655b6`**，r2 批不重打 tag） | **tag `v2.27` 不存在** | `git tag --list v2.27` → 空输出；`git rev-parse v2.27^{}` → `fatal: ambiguous argument 'v2.27^{}': unknown revision or path not in the working tree.`；`git tag -l 'v2.2*'` 止于 `v2.26` |
| 2 | HC-0 ①：`dev/v2.27`（`db655b6`，含 r2 轮 R1…R8）**合入 `main`** | **未合并**：本地 `main` 与 `origin/main` 均停在 `ec15356`（= v2.26-r2 R6b），`dev/v2.27` 为唯一在途分支且领先 18 笔（`main...dev/v2.27` = 0/18，与计划 §1.0 ① 路/§7② 起草期实测一致） | `git merge-base --is-ancestor db655b6 main` → **exit 1（非祖先）**；`git rev-parse --short main` → `ec15356`；`git rev-parse --short origin/main` → `ec15356`；`git log --oneline snap-2.27-start..HEAD \| wc -l` → 17（+ 快照笔本身 = 18 面） |
| 3 | HC-0 ③：**CI 七 job success + Release Latest = v2.27 四附件 + `dev/*` 清空** | **全部未兑现**：远端无任何 v2.27 相关 run（最新 run 仍是 2026-10-07 的 v2.26-r2 R6b CI main），Release Latest 仍是 **v2.26**，本地分支 `dev/v2.27` 仍在 | `gh run list --limit 3` → `completed success v2.26-r2 R6b…` / `v2.26 P2-5(b)(C8)…` ×2（无 v2.27 项）；`gh release list --limit 2` → `v2.26  Latest  v2.26  2026-10-07T10:26:28Z`；`git branch --list "dev/*"` → `dev/v2.27` |
| 4 | AC-0：交付首行须登记上述三项**实测值**（tag 输出 / `rev-parse v2.27^{}` SHA / `main` 含 `db655b6` / Release Latest 读数），「任一为空 = 本版未开工合法性，BLOCKED 件号在案」 | 三项实测值**全部为空/未达**，故本版尚无 DELIVERY 首行，本 BLOCKED 件即 AC-0 要求的件号承载 | 本文件 §1 表列读数；`sh tools/docsync.sh` → `pass=372 fail=0`（与计划 §7② 起草期读数同值，门本身未被扰动，见 §2） |
| 5 | （相关但非本前置）v2.27 **r2 轮复评记录** `versions/v2.27/REVIEW-r2.md` 不存在（A2 未出）；据此计划 §1.1 行 17 判定 REQ-6/8/9 **不代转**「已完成」 | 实测 `versions/v2.27/` = 四件（`PLAN.md`/`DELIVERY.md`/`DELIVERY-r2.md`/`REVIEW.md`），缺 `REVIEW-r2.md` | `ls -1 versions/v2.27`；计划 §1.0 ③ 路与 §6「入仓批（预申报）」按「若 A2 已产出」条件式表述，与本 BLOCKED 无冲突 |

## 2. 已停手的位置

**未开始任何 v2.28 动作**（§6 顺序规则的 ② 及之后全部未执行）：

- 未建分支 `dev/v2.28`；未打 `snap-2.28-start`；未做入仓批提交（`BACKLOG.md`/`ROADMAP.md` 规划者预改与 `versions/v2.28/` 三件套之一 **保持未入库原样**）；
- 零文件内容改动、零门改动、零板侧动作（G474 本次未触碰；REQ-5/REQ-7 的板侧实跑与 §6「板不可达合法路径」均未启用）；
- 本工作树状态：`git status --short` = `M BACKLOG.md`、`M ROADMAP.md`、`?? versions/v2.28/`（+ 本件），当前分支 = `dev/v2.27`（HEAD = `db655b6` = v2.27-r2 R8）；
- 已完成部分（只读核验）：通读 `versions/v2.28/PLAN.md` 全文 199 行（重点 §2 HC 全条与 §5 可裁量项/不可偏离）；核对计划头部状态 = **已批准-冻结** ✓；复核 §7 起草期六项与 §7② 实测读数逐项在案；本机复跑 `docsync` = **372/0**（与计划预置起点值同值，规划者/执行者均未扰动门）；核对本版允许面与 v2.27 实况无冲突（`et_config.h` 现值 27/`0x021B00`、开关 32、模块 35、用例基线 520 四形态 520/520/527/521、体积三行读数——均与计划 §7 文末预置数值一致）。

## 3. 建议（可选）：修正方向

HC-0 三项是**动作**而非决策，任选一条路径即可解阻：

**路径 A（Owner 侧执行，推荐；约 3–5 分钟 + CI 约 1–2 分钟）**

```sh
git checkout main
git merge --ff-only dev/v2.27                 # main: ec15356 → db655b6（a02f98a 随之内联可达）
git tag v2.27 a02f98a                         # ★ 必须落 a02f98a，不是 db655b6（v2.27 评审 §8 前置①）
git push origin main
git push origin v2.27                         # 触发 release.yml（v2.26 同款：Latest + 四附件）
gh run watch                                  # 七 job 全绿
git branch -d dev/v2.27                       # dev/* 清空（AC-0/HC-0 ③）
```

要点：① `a02f98a` 在 ff 合并后已是 `main` 的祖先 ⇒「发版 tag 在全部改动定着 `main` 之后」与「⑧/verify 定义时点」两条同时成立；② 若把 tag 打在 `db655b6` 或更晚，v2.27 的 (b)/(d)/token 三锚行随之后移，交付文档 ⑧ 与实测失配（v2.27 主批 §7.5 与评审 §8 即为此设防）；③ r2 轮不重打 tag，沿 v2.26 先例。

**路径 B（Owner 明示授权执行者代做 push/tag/Release）**

与 HC-0 括号「push/tag/Release 不由本 Agent 代做」冲突 ⇒ 属冻结计划约束的放宽，需 Owner 批准并**追加到 `versions/v2.28/PLAN.md` 文末「变更记录」**（铁律 2 的唯一通道；执行者不擅自改计划）。批准后我以 B1 身份按路径 A 的同一序列执行，并在 v2.28 交付 §2 AC-0 行登记三项实测值、在 §3 偏差表「计划面外授权」列（本版五列形态首例）登记该授权来源 = 本件 + 变更记录行。

**顺带提请（不属本前置，不阻塞）**：v2.27 的 **r2 轮复评**（A2 独立会话，可只审 delta）仍缺记录件 `versions/v2.27/REVIEW-r2.md`。它不阻塞 v2.28 开工（HC-0 未含此项，计划 §6 入仓批对它就是条件式「若 A2 已产出」），但决定 REQ-6/8/9 何时转「已完成」（计划 §1.1 行 17 判据）。若复评在 v2.28 入仓批之前出件，则该档随本版入仓批一并入库，零额外动作。


## 4. 解除记录（2026-10-10，随 v2.28 入仓批入库）

**裁决 = 本件 §3 路径 B**：Owner 2026-10-10 对话选择「授权我代做发版（推荐）」，即 HC-0 原文「push/tag/Release 不由本 Agent 代做」在 **v2.27 发版本批**这一前置件上放宽；授权与归因载体 = `versions/v2.28/PLAN.md` 变更记录（铁律 2 追加通道），本版 v2.28 自身的发版步**不随之放宽**。

HC-0 三件实测（命令与输出逐件，2026-10-10）：

| # | 前置项 | 实测 | 证据命令 |
|---|---|---|---|
| ① | `dev/v2.27` 合入 `main` | `main` = **`5d33d2f`**（由 `ec15356` 两次快进：`68ca5bd` 发版批 20 笔 → `5d33d2f` 发版后补丁 P-1）；`db655b6`（r2 末笔）**在 main 内** | `git merge-base --is-ancestor db655b6 main` → **exit 0**；`git rev-parse --short main` → `5d33d2f`；`git rev-parse --short refs/remotes/origin/main` → `5d33d2f`（本地与远端同值） |
| ② | tag `v2.27` 落 **`a02f98a`**（不是 `db655b6`） | **成立**：`git tag --list v2.27` → `v2.27`；解引用 = **`a02f98a`**（轻量标签，沿 `v2.26` 形态）；`git diff --shortstat snap-2.27-start v2.27` = **36 files / 2770 / 64** = 主档 §7.4⑧ (b) 全量锚值同值（定稿时点真值不随发版后移） | `git tag --list v2.27` / `git rev-parse --short 'v2.27^{}'` / `git cat-file -t refs/tags/v2.27` → `commit` |
| ③ | CI success + Release Latest 四附件 + `dev/*` 清空 | **成立**：补丁后 CI run `38046749929`（main push）**七 job 全 success**；Release **`v2.27` = Latest**，附件 **4** 件（`demo.exe` / `SHA256SUMS` / `stm32f103_demo.bin` / `stm32f103_demo.elf`，与 v2.26 同形态）；`dev/v2.27` 已删（`Deleted branch dev/v2.27 (was 68ca5bd)`），现仅存本版分支 `dev/v2.28` | `gh run view 38046749929 --json jobs` / `gh release view v2.27 --json tagName,assets` / `gh release list --limit 1` / `git branch --list "dev/*"` |

**发版过程中新出的缺陷（本件范围外，如实登记）**：首次 main push（`68ca5bd`）触发的 CI run `38045409236` 在 **asan 步骤 FAIL**（`stack-use-after-scope` @ `debug/et_metrics.c:328`，命中 `test/test_metrics.c` 用例内层块句柄）⇒ 修法 = 补丁 `5d33d2f`（测试句柄上移函数作用域 + `et_metrics_bind` 生命周期契约补注；零库面行为变化、零签名变化、用例数仍 520）。Owner 同批判定 = 该笔 ff 进 main 并 push（推荐项）+ **已发布 v2.27 发布物不动**（库实现未改，不构成产物面缺陷）。归因要点：本机 MinGW 无 libasan（Makefile:102-104 既有声明），asan 门 = Linux-CI-only ⇒ v2.27 交付期「四形态 520 全绿」对该缺陷类**天然不可见**，且第二次断言读陈旧栈字节 = 假绿；同族先例 = v2.5 `va_list` 的「只在 Linux 报警」。该缺口作为 **v2.28 复评输入**留待 A2 判定（本版 P0 批未含「发版前跑 Linux asan」的动作项）。

**REQ-6/8/9 状态位（未代改）**：本复评通过 + 发版三件定着 ⇒ `versions/v2.28/PLAN.md` §1.1 行 17 的判据前件「v2.27 评审通过」已成立，但该计划变更记录初行写明「状态位**不代转**」，且本版任务面无相应条目 ⇒ 执行者**保持 BACKLOG 现状不动**，改判归 Owner（铁律 9）。
