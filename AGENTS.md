# AGENTS.md

## 这是什么

Embedded_Tools 是一套面向嵌入式 MCU 的 C99 组件库：零动态内存、多实例句柄化、分层单向依赖、PC 可全量单测。当前 **v2.30.0**（API 冻结版本，MINOR 只追加），36 个模块，覆盖 core / algorithm / sys / storage / protocol 与各 port 移植。

## 怎么跑

- host 单测：`make WERROR=1 test`（Windows 无 make 用 `mingw32-make WERROR=1 test`）
- 双几何回归：`make test-g4`；1K 块变体：`make test-1k`；查表变体：`make test-tab`
- 配方示例：`make ex`（六个自检式示例，任一 FAIL 即红）
- 内存门：`make test-asan` + `make test-asan-1k`（Linux CI；Windows 本机 `CC=clang ASAN_DLL_DIR=<LLVM asan 目录>`，见 README「内存检查门」）
- CMake 接入面：`cmake -B build-cmake -G Ninja -DCMAKE_C_COMPILER=gcc -DET_WERROR=ON -DET_BUILD_EXAMPLES=ON && cmake --build build-cmake && ctest --test-dir build-cmake`
- 文档/登记门：`sh tools/docsync.sh`（期望末行 `docsync: pass=N fail=0`）、`sh tools/docref.sh <交付文档路径>`、`sh tools/gatecheck.sh <主.次版本号> --verify <交付文档路径>`（末笔时跑，红即不放行 tag）
- 体积门：`sh tools/sizecheck.sh`（发布前本机跑，期望 `pass=3 fail=0`）
- 板侧打包：`python tools/pack_image.py --in <body.bin> --out <img.bin> --ver <N>`

## 本项目流程

- 现行规则：`PROCESS.md`（轻量软件开发流程规范 v2）+ `prompts/`（A1/B1/A2/B2 角色）+ `tools/` 门禁；发布 checklist 在 README。
- **流程即判据，不要做目录重排**：历史交付/评审文档按文件名和位置被 `docref.sh` / `gatecheck.sh` 定位，改名或移动会让判据静默失效。
- 根目录 `v*.md` 与 `versions/` 旧档 = 历史记录：只读，不作为当前行动依据；除门禁与追溯外不要读。
- 当前版本：**v2.30.0**；当前版本记录：`versions/v2.30/`。
- 入口约定：本文件 = 唯一上下文入口；`TODO.md` = 唯一待办入口。

## 红线（不可违反）

1. **先判定，后规划**：任何版本计划必须先过阶段 0 的立项判定与价值声明；不立项合法且被鼓励；禁止为闭环而闭环。
2. **计划冻结**：批准后执行者只打勾不改内容；变更仅两个通道——PLAN 文末「变更记录」追加（Owner 批准）或 DELIVERY 偏差表。
3. **强制处置**：REQ 待评估项与 CO 存量在阶段 0 必须逐条给结论（纳入 / 暂缓 / 拒绝 + 理由）；不允许静默丢弃。
4. **git 锚定**：开发前先打基线 tag；审核先 `git diff` 后读文档；「声称通过但无证据」的 AC 默认判不通过。
5. **审核独立**：审核者 ≠ 执行者；评审全程不向仓库产生提交。
6. **来源可追溯**：计划中每个任务必须标注来源（REQ-n / CO-n / M-n / PF-n / 用户直连）；无来源的任务不允许存在。
7. **阻断停手**：发现计划有阻断性缺陷，停止开发并写 `BLOCKED-n__描述.md`；不硬扛、不擅自绕道。
8. **-r3 熔断**：同一版本第 3 轮返工仍未通过，停止同口径返工——出计划修正，或请 Owner 仲裁。
- 附加不可动面：不改门禁脚本与 CI（独立任务需单独授权和验证）；push / tag / Release 由 Owner 执行，agent **不做任何远程提交（含 `git push`、`--all`、`--tags`）**。

## 当前状态（每次收工更新两行）

- 已完成：v2.29 与 v2.30 均已发版（tag `v2.29`=`810d355`、`v2.30`=`05f91ce`，main=`05f91ce`，两个 Release 各四附件）；首次 main CI 暴露的 5 处缺陷已修（`27727cf` 六件、`05f91ce` ci.yml），@`05f91ce` CI 7/7 全绿；REQ-7 真固件双单达成（1K + 升级期静默 + 57600），`et_params` 落地；2026-10-23 完成 115200+1K 复测（87/87 ACK、0 重试、`ore=0`；板侧已恢复 57600）。
- 下一步：① v2.31 立项时补 25 份历史交付档登记行并重取 ⑧ 锚点（当前红态，禁回填）；② 决定是否切换 115200（需固化上位机纪律并同步 ROADMAP/池 H）；③ 确认 `versions/v2.30/REVIEW-r2.md` 是否补做 A2 复评。

## 已知问题

- 2026-10-24 发版：tag `v2.30` 落 `05f91ce`（含定稿链后 5 处 CI 修复）；`gatecheck --verify` 锚点未重取（绿读数停在 `f78937b`），`(d)` = OK 0 / 未登记 25，交 v2.31 登记轮吸收（登记 = `versions/v2.30/DELIVERY-r2.md` §3 偏差 8）。
- v2.29 Release（`810d355`）含两处已修缺陷（recipe 示例 `slot_base` 缺初值、`et_assert` 的 LOG 关断守卫）；Release workflow 不跑这两面，故其发布页仍为绿。
- **115200 + 1K 已于 2026-10-23 复测通过**（修正主机纪律；confirm/rollback 各 87/87 ACK、0 重试、9.03 s、`ore=0`；证据 = 库外 `D:/code/STM32CubeMX/G474VET6_ET_TEST/build/v3h-115200-evidence/dual_walk_1k_115200.log`）。当前默认/可用形态仍 = 1K + 升级期静默 + 57600；是否切换 115200（需固化上位机纪律并同步 ROADMAP/池 H）待 Owner 决定。
- 根目录三件 BLOCKED 的头部状态仍写「待裁决 / 受阻在案」，但对应问题已由 v2.29 / v2.30 消解：`BLOCKED-2__v2.28…`（转 v2.29 路径 A）、`BLOCKED-2__v2.29…`（v2.30 P1 双单达成）、`BLOCKED-3__v2.28…`（§4 更正章，申报面作废）。按执行卡原文件冻结，不改不搬。
- `versions/v2.30/REVIEW-r2.md` 未见入库；r2 档提到「复评记录再入库」，是否需补做 A2 复评待 Owner 确认。
- 当前 HEAD 的 `gatecheck --verify` = 红态（⑧ 锚点停在 `f78937b`；`(d)` 25 份「未登记实质变更」）—— 由 `a3530e2`/`09128e9`/`77c44ab`/`27727cf`/`05f91ce` 触碰非白名单文件所致；按 CO-12(v2.28) 交 v2.31 新末笔重取，禁以红态值回填。
