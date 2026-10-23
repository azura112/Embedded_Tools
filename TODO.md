# TODO

- [x] 提交 `AGENTS.md` + `TODO.md` + `.gitignore` 增行（2026-10-23 Owner 授权提前于 v2.30 发版执行；仅本地提交，不推送。偏差：本笔为 `f78937b` 之后的新提交且三件非 docref 白名单 ⇒ (b) 锚后移、主档/十档登记链预期转红，`gatecheck --verify` 预期红 —— 按 CO-12(v2.28) 在新末笔重取读数，由下一版登记轮吸收；来源：2026-10-23 迁移执行卡）
- [ ] 发版顺序①（Owner 执行）：push `main` + tag `v2.29`，随后发 v2.29 Release（来源：v2.30 DELIVERY §6-1 / HC-0②）
- [ ] 发版顺序②（Owner 执行）：v2.30 合入 `main`，tag `v2.30` 落 `f78937b`（r2 定稿链末笔），跑 CI/Release（来源：versions/v2.30/REVIEW.md §8 / build/final-gates-v230.log）
- [ ] 发版后按 CO-12(v2.28) 在新末笔重取 `gatecheck --verify` / `docref` / (b)(d) 读数（来源：versions/v2.30/DELIVERY-r2.md §6）
- [x] 复测 115200 + 1K 块型（2026-10-23 通过：修正主机纪律 + demo 临时 115200，confirm/rollback 各 **87/87 ACK、0 重试、9.03 s**，传输期 **`ore=0`**（`rx=89717/89558`）；证据 = 库外 `D:/code/STM32CubeMX/G474VET6_ET_TEST/build/v3h-115200-evidence/dual_walk_1k_115200.log`。板侧 demo 已恢复 57600 兜底并回烧；是否将 115200 设为默认、固化上位机纪律并同步 ROADMAP/池 H，待 Owner 决定；来源：v3 池 H / v2.30 DELIVERY §6-2）
- [ ] Owner 确认三件 BLOCKED 的实际结案状态，并按执行卡冻结原文件（不改、不搬；来源：根目录 `BLOCKED-2__v2.28…` / `BLOCKED-2__v2.29…` / `BLOCKED-3__v2.28…`）
- [ ] 确认 `versions/v2.30/REVIEW-r2.md` 是否需补做 A2 复评并入库（来源：versions/v2.30/DELIVERY-r2.md §6 / 目录现状）
- [ ] 发版后清理已合并的 `dev/v2.29` 与已合入的 `dev/v2.30`（Owner 执行；来源：迁移执行卡第 2 步；须在推送后）
