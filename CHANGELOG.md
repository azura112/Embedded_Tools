# Changelog

Embedded_Tools 版本变更记录。格式沿 [Keep a Changelog](https://keepachangelog.com/);
每版条目自该版交付文档"重点概况"提炼,**随交付更新**(模板:Added/Changed/Fixed/挂账)。
版本路线全文见各版 `v<版本>开发计划/交付` 文档。

## [Unreleased]

## [2.8.0] — 2026-09-26 · **v2.8 — 冻结面基线滚动与门空隙收口**

治理版（冻结契约下零公开面变化）：无新模块（模块数维持 **34**）、无新增开关、无签名变更，`apidump --diff` 两轮基线读数均为 **新增 0 项 / 修改删除 0 项**。

### Added
- **冻结基线补归档（`CO-2(v2.7)` ★ 治理核心）**：`docs/API_INVENTORY_v2.6.md`（v2.6 发布时缺失、v2.7 亦漏列，滞留跨两版）由 `git worktree` 于 `tag v2.6` 用当前工具真生成补归档（与 v2.5 归档行尾归一比对 = 仅 `ET_VERSION_MINOR` 一行差，公开面 0 增改）；发版点再归档 `docs/API_INVENTORY_v2.7.md`；`apidump` 默认基线两轮滚动读数均 0/0；**"基线滚动"写入 `docs/开发计划模板.md` §7 第⑥项**（根治连续两版漏列的结构性原因，`docs/评审记录模板.md` 增评审侧复核节）。
- **门空隙收口**：`docref` 增交付文档 §4 清单逐 SHA 可达性断言（封死 `CO-1(v2.7)` 悬空提交形态）；`docref` 的 `END==HEAD` 硬判据迭代为**白名单式判定**（评审记录/计划等良性提交不再使门设计性红，CHANGELOG/交付文档未登记变动仍必红，`CO-6(v2.7)`）；`apidump` 的 `item_lines` 显式 `tr -d '\r'` 抗行尾（`CO-8(v2.7)`，副作用升级为契约）+ Windows 临时目录回退显式告警 + 生成条目数守卫（防清单被残缺生成覆盖，`CO-9(v2.7)`）；README 基线表述全量装门（`CO-5(v2.7)`）。

### Changed
- 版本 2.7.0 → **2.8.0**（`0x020700`→`0x020800`）；`apidump` 默认冻结基线 v2.5 → **v2.6**（发版点 → **v2.7**）；docsync 断言 304 → **306**（+2 模板第⑥项/README 项数一致断言；−2 基线三字面量断言并为单一来源动态断言；+1 版本链 2.8.0 节；+1 交付文档本体进入覆盖率行循环）。
- **解析边界复评落档（`CO-7(v2.7)`/`CO-10(v2.7)`，结论见 API_GUIDE 5.7）**：`0x10` 帧"双假设定长"能力恢复的评估结论与从站读帧 8 字节窗口的取舍裁决入册，防下版重复评估。

## [2.7.0] — 2026-09-25 · **v2.7 — 从站解析对称化与板上自测收口**

冻结契约下**纯内部修复 MINOR**（`apidump --diff` 对 v2.5 基线：**新增 0 项 / 修改删除 0 项**）；无新模块（模块数维持 34）、无新增开关、无签名变更。

### Added
- **selftest 内建套件 20 → 22（P1-1/P1-2，关闭 `CO-14(v2.6)` 与 `CO-10(v2.5)`）**：新增 `modbus`（从站 + 主站**解析边界**纯逻辑冒烟：伪长度噪声重同步、定长严判、逐字节重同步、`crc_err` 口径）与 `log`（**修饰面**冒烟：域宽/补零/左对齐/精度、`%c` 域宽、`j/t/L` 消费与占位、未知转换字符不消费）两个套件 —— **主站与 `et_log` 此前在板上零冒烟覆盖**。缓冲全为文件级静态（主站双缓冲 2×`ET_MODBUS_ADU_MAX` = 512B 为 RAM 增项主体，见体积表）。
- **`test/test_modbus.c` 增 10 例解析边界矩阵（P0-3）**：长度域不自洽帧不伪造帧长、**伪写入帧噪声后接真读/真写请求仍被正确应答**（评审 E10 形态）、异站地址噪声头、广播+未知功能码噪声头、纯噪声全丢弃且不污染 `crc_err`、单次 feed 内"坏帧+真帧"粘包、CRC 坏且帧首非本站不计 `crc_err`、单播未知功能码仍保留给静默路径、长度域自洽但帧未齐时等待而非误判噪声。
- **`docs/API_GUIDE.md` 5.7 从站统计口径表（P0-4，HC-3）**：`frames` / `responses` / `exceptions` / `crc_err` / `addr_mismatch` / `discarded` 的**逐条加一条件**，与实现差集为空（逐条由 `test/test_modbus.c` 固定）。

### Changed
- 版本 2.6.0 → **2.7.0**（`0x020600`→`0x020700`）；用例 487 → **497**（×双几何；**1K 变体 498**、Tab 形态 **504**、G4 几何 497）；`modbus` 23→33。
- **`et_modbus` 从站解析边界对称化（P0-1/P0-2，`CO-5(v2.6)` ★ 核心）**：① `expected_len()` 对 `FC_WRITE_MULTIPLE` **先校验字节数域**（`b[6] == 2*qty`）再返回帧长，不符返回"不可判定" —— `b[6]` 不再先于 CRC 被信任（旧实现下噪声只需伪造一个 `b[6]` 即可让解析器整段前进、吞掉紧随的真请求；评审探针 E10 实测）；② 缓冲首部**已不可能**是本站请求起点时（帧首非本站/广播地址，或 0x10 长度域不自洽）**逐字节重同步**；③ CRC 失败只用"**与本站期望同形**"（帧首 == 本站从站地址且功能码/长度域自洽）的候选帧计 `crc_err`，其余计 `discarded` 并重同步 —— 与主站 v2.6 判据**同一语义**。**行为修正（语义面）**：`crc_err`/`discarded` 口径收紧（字段与布局未动，已在 API_GUIDE 5.7 与交付文档 §2 登记）；`bc` 与 `qty` 不自洽的 0x10 帧**不再产生异常 0x03 应答**（改为逐字节重同步 —— HC-2 明文要求；该能力经静默路径亦不可达，因字节已在 feed 内被重同步清掉），既有用例 `mb.exc_illegal_value_wr` 相应改写为"不产生应答 + 不进 `crc_err` + 逐字节丢弃"。
- **`et_modbus_master` 读路径死代码清理（P2-1，`CO-9(v2.6)`）**：删除 `handle_frame()` 中**不可达**的"字节数不符 → discarded"分支（改为 `ET_ASSERT` 不变式断言；默认展开 `((void)0)`，零开销）与 `is_expected_shape()` 中**到达点恒真**的读分支子句 —— **行为零变化**（全量 497 例全绿）。
- **`L` + 整数转换的语义限定（P2-2，`CO-10(v2.6)`，采"文档限定"分支）**：`docs/API_GUIDE.md` 8.1 与 `et_log.h` 头注显式声明 `%Lu` / `%Ld` / `%Lx` 属**未定义行为**（C 标准中 `L` 只对浮点转换有定义）：库按 `long double` 消费一次 + 占位。本机（Windows x64）实测不错位（v2.6 探针 E13），但 x86-64 SysV 下 `long double` 属 memory-class、`va_arg` 可能不推进整数实参槽 → **不保证可移植语义，请勿使用**（64 位整数用 `%lld`）。
- 数字回刷：README（特性表/结构树/测试与质量门/检出边界）、`architecture.md`（套件数/用例数/"连续十七版"/机制门行）、`API_GUIDE`（5.7/8.1/8.4/9.1）、`getting-started`、`renode/smoke.sh` 断言（`SELFTEST: 22/22`）、`port/*/README` 体积表、docsync 自指计数、`et_config.h` 版本宏与 `docs/API_INVENTORY.md`（重生成，由 `apidump --check` 守护）。

### Fixed
- **从站侧同源缺陷（`CO-5(v2.6)`，与 v2.6 主站侧同一类）**：v2.6 只修了主站（改 A 漏 B），从站侧"字节数域先于 CRC 被信任 + CRC 失败后按未受信长度整段前进"仍在。本版对称化后，评审 E10 形态（11 字节伪写入帧噪声 + 真读请求）在从站上**真请求被正确应答**且 `crc_err` 不增长。

### 挂账（如实声明）
- **`snap-2.7-start` 的远端推送曾受阻后恢复**：首次多次遇出口代理 `502`，**重试后成功** —— 远端与本地同为 `724d089`（`gh api` 核验）；无副作用由探针 tag 实测（推送不产生新 run、不产生 Release —— `snap-*` 不匹配 `release.yml` 的 `tags: [v*]`）。见交付文档 §3 D-1。
- **`CO-15(v2.6)` bench 相对比值化未做**（P3-1 时间盒，计划明示"不阻塞发布"）：显式声明 + 再记 v2.8，触发 = 出现"需要跨版性能回归判别力"的实际诉求（交付文档 §8.3）。
- **板侧 demo 的命令行 `\n` 残留未清**（`CO-8` 已机械定位到来源，见实机记录 §15.4）：修法一行级（进入阻塞式主站事务前清空 RX 环残余），本次**保留证据原样**；记 v2.8 板侧待办（**库外**，不影响库内门与发布）。
- **`CO-11(v2.5)`（USART2 独立口 / 共享口地址不符静默的板侧验证）再次暂缓**：bench 仅一路 USB-TTL，触发条件未变。
- **本机 ASan 执行路径未启用**（无 WSL/MSYS2）：`make test-asan` / `make test-asan-1k` 由 Linux CI 常设执行，**未声称本机已跑**。

## [2.6.0] — 2026-09-24 · **v2.6 — 解析边界清偿与检出能力补强**

冻结契约下**纯内部修复 MINOR**（`apidump --diff` 对 v2.5 基线：**新增 0 项 / 修改删除 0 项**）；无新模块（模块数维持 34）、无新增开关、无签名变更。

### Added
- **跨文档计数一致性门（P0-2，`CO-3`/`CO-4` 的机制化根治）**：`tools/docsync.sh` 增两条断言 —— README 的**用例数与 examples 数**（单一来源）须与 `docs/architecture.md` 机制门行、`docs/API_GUIDE.md` 8.4 的对应数字**逐值一致**，且 examples 数须等于 `examples/ex_*.c` 实测文件数；自指断言同时把 `architecture.md` 机制门行的 `docsync N 断言` 纳入 `expect` 对账。**v2.5 的三处漏刷（"四例"/"docsync 210"/"398"）从此有门守护**。
- **`tools/docref.sh`（P0-3，`CO-5` 的机制化）**：交付文档 §4 自指核验 —— 提取"记录范围 `git diff --stat <base>..<end>` = **N files / M insertions / K deletions**"声明，与实测**逐值**比对（`<base>`/`<end>` 须可解析）。CI 不跑（浅检出无 tag 历史），属**发布前本机门**（沿 sizecheck 先例）。根治 v2.5 D-10 的"§4 滞后自身提交"。
- **`docs/开发计划模板.md`（P0-4，`CO-12`）**：六节骨架 + **起草期核验清单四项**（① 内部 tag 一律 `snap-<ver>-start` 且触发面须实测；② "无副作用/不会触发"类断言起草期必须实测；③ 板侧落点写**库外私有全路径**；④ **AC ↔ 冻结 API 形状互洽**检查）；README 发布 checklist 第 10 条指向；`docs/评审记录模板.md` §3.1 增"AC ↔ 形状互洽"复核项。**（v2.6-r2 增至五项**：⑤ 证据落点章节号 + 占位节承接标注，来源 `CO-11(v2.6)`）
- **`make test-1k` / `make test-asan-1k` 常设目标（P1-6，HC-10）**：1K 块变体此前靠手工 gcc 命令，现为 Makefile 目标；ASan 门自 v2.6 起**覆盖 1K 变体**，CI 增两步。
- **host 零警告硬门（P2-1，`CO-13`）**：`make WERROR=1 <目标>`（Linux）与纯 gcc `-Werror`（Windows job）—— 把"只有 ARM 交叉编译有零警告门"扩到 host 侧（CI 两个 job 同用；本机 `WERROR=1` 全绿实测）。
- **README「质量门与检出边界」小节（P2-2）**：CI 硬门清单（零警告/ASan/冻结面/文档同步/覆盖率/docbuild）+ **本机检出边界如实声明**（MinGW 无 `libasan`、clang ASan 运行时本机不可用、无 WSL、无 MSYS2）+ 触发条件。
- **7 例主站噪声用例**（`mbm.noise_*`）+ **6 例 `et_log` 修饰面用例**（`log.char_field_*` / `log.len_mod_*`）。
- `docs/bench.md` **口径声明（P3-1 轻量分支）**：本表数字**仅供量级参考、不作回归判据**（v2.5 评审实测同机同日差幅最大 59%），性能结论须附同轮原始读数。

### Changed
- 版本 2.5.0 → **2.6.0**（`0x020500`→`0x020600`）；用例 474 → **487**（×双几何；**1K 变体 488**、Tab 形态 **494**、G4 几何 487）；`modbus_master` 25→32、`log` 24→30。
- **`et_modbus_master` 读应答定长判据（P1-1，`CO-6` ★ 核心）**：收到读类应答时，其**字节数域必须严格等于 `2*exp_qty`**（单事务语义下主站已知期望寄存器数），否则判为噪声 → **逐字节重同步**；`crc_err` 只统计**与在途事务严格同形**（帧首=本站地址且功能码/字节数域一致）的候选帧 CRC 失败。**行为修正**：① 半双工单线上"主站自身请求回显"（形如 `11 03 00 00 00 04 …`）不再伪造帧长、不再把真应答切走（v2.5 行为只能靠超时重发收场）；② `crc_err` 口径收紧（字段与签名未动，属语义修正，已在 API_GUIDE 5.8 与交付文档 §2 登记）。
- **`et_log` 修饰面与承诺对齐（P1-3/P1-4，`CO-7`/`CO-8`）**：`%c` 的**域宽与左对齐**真正生效（按 C 语义精度对 `%c` 不起作用）；长度修饰 **`j`/`t`/`L`** 从"未知转换字符（不消费）"移入"已知不支持 → 消费 `intmax_t`/`ptrdiff_t`/`long double` + 完整字形占位"（`%jd` → `<?jd>`、`%Lf` → `<?Lf>`），消灭错位的最后口子。`et_log.h` 头注、API_GUIDE 8.1 规格表、`test/test_log.c` 三者**差集为空**（HC-3）。
- **`tools/apidump.sh` 环境适配修复（本版新增，工具缺陷）**：① Windows/MSYS 下 `TEMP` 为 `C:\...` 形式时，gawk 的 `>> path` 把反斜杠当转义 → 提取文件写到别处 → 清单**误报漂移**（且无参数运行会把清单覆盖成残缺版）→ 改用**仓库内相对临时目录**；② 循环内逐文件 `rm`（最多 108 次）在带"安全删除"wrapper 的 shell 封装下每次耗时数十秒 → 脚本从**秒级劣化到小时级** → 改为由 `trap` 一次性清理。
  **（v2.6-r2 更正）** 原记"修复后本机 `--check` **21s**完成"**不可复现**（`CO-13(v2.6)`）—— 本机实测（PortableGit sh 5.3.15，`sh tools/apidump.sh --check`）**4m24s**（修复前）/ **`4m18s`**（修复后）、`--diff` **`4m25s`**；耗时量级由 `gen_body` 的按头文件逐个 awk 调用决定，环境注记见 `-r2` 交付文档 §7。**另**：本次修复**未触及行尾敏感性** → `--check` 在交付平台恒红（`CO-1(v2.6)`），由 `v2.6-r2` 的 G0′-1（比对前行尾归一化）修复。
- **apidump 基线滚动（P0-1）**：归档 `docs/API_INVENTORY_v2.5.md`（433 项），默认基线切至 v2.5；v2.0~v2.4 归档只读保留。
- 数字回刷：README（特性总览/结构树/测试与质量门/检出边界）、`architecture.md`（"五例"、"docsync 296 断言"、"连续十六版"）、`API_GUIDE` 8.1/8.4/5.8、`bench.md`、`port/*/README` 体积表、docsync 自指计数（291 → **296**）。

### Fixed
- **v2.5 遗留的清单残缺风险**：`docs/API_INVENTORY.md` 在受限 shell 环境下被无参数 `apidump` 覆盖为残缺版的路径已被消除（见上条 TMPD 修复）；本版重新生成并与入库版比对，**差异仅版本宏一行**（`ET_VERSION_MINOR 5→6`），其余逐字节一致。

### 挂账（如实声明，**v2.6-r2 已更正**）
- **板侧项已执行（2026-09-24 板接入后补齐）**：v2.5 的**板上逐字节走单**（主站读 / 写回显 / 丢包重发×3 / 异常 / 广播 + `et_log` 域宽行 + v2.4 从站分流复跑）记入 `移植stm32实机记录.md` **§13**；v2.6 的**板侧同步 / 构建 / 下载 / 板上回归**（`diff -rq` 全 OK、0 warning、FLASH 75144 B / RAM 5960 B、横幅 `v2.6.0 (0x20600)`、`AT+VER → ver=2.6.0`、`SELFTEST 20/20`、`SELFSTOR PASS`、`SIMUPGRADE 3 → slot 1 CONFIRMED`）记入 **§14.1**。`tag v2.5` = `25f3316` **已打并已推送**，Release v2.5 四附件在位（`gh release view v2.5`）。
- **唯一未取得的板侧证据 = `CO-6` 的"修复前/后"对照**：对照实验（从站**完全不应答**）显示 `disc=0 / crc=0` → 本板 USART1 为 **TX/RX 独立**，**不存在**自身请求回显，`CO-6` 的受害形态在本硬件**不成立**（§14.2）；该修复在本板属**防御性**修正，正确性由 host 侧 7 例（`mbm.noise_*`）承担。**不伪造任何板上结论。**
- **`selftest` 维持 20**（未扩至 21）：按 HC-12「不扩须显式声明并给理由」分支处置，故 `CO-10(v2.5)` **未关闭**（主站/日志在板上零冒烟覆盖）→ 记 **v2.7 必办**（`CO-14(v2.6)`）。
- **`CO-11` 再次暂缓**（USART2 独立口 / 共享口地址不符静默的板侧验证，硬件阻塞同前；触发 = 第二路 USB-TTL 或双板对连）。
- **本机 ASan 执行路径未启用**（P3-3：无 WSL/MSYS2，触发条件未满足）；`make test-asan` / `test-asan-1k` 于 Linux CI 执行，**未声称本机已跑**。

> **更正声明（2026-09-25，`v2.6-r2` 返工轮 / `CO-2(v2.6)`）**：本节**原表述**为
> "板侧项**均未执行**、`tag v2.5` **未打**" —— 该表述与事实**相反**，成因是板接入（2026-09-24）
> 后旧文本未随实机记录 §13/§14 回刷。更正方式：① 本 CHANGELOG 段重写（下条更正生效处）；
> ② 已发布 Release v2.6 正文用 `gh release edit v2.6 --notes-file …` **重注**
> （正文仍由本节按 `release.yml` 的同一 awk 规则提取，双源一致）；**未重打 tag**（评审明示禁止）。
> 同批修正 D-6 的"**21s**"耗时读数（见 Changed 段 `tools/apidump.sh` 条）。

### v2.6-r2 返工轮（2026-09-25，前置门 G0′，**不含新开发**）

> 来源：`v2.6评审记录__解析边界清偿与检出能力补强.md`（结论**返工**）§6.1。**未重打 `v2.6` tag**；
> 已发布 Release v2.6 正文以 `gh release edit v2.6 --notes-file …` 按本节 + 上方更正后的
> `<### Added/Changed/Fixed>` 全节重注（提取规则与 `release.yml` 同一 awk）。

#### Fixed
- **`apidump --check` 行尾敏感（`CO-1(v2.6)`，发布阻塞级）**：本仓 `core.autocrlf=true` 且无 `.gitattributes` → 入库清单 `docs/API_INVENTORY.md` 在工作树检出为 **CRLF**，而 `gen_body` 产出 **LF** ⇒ 字节级 `diff -q` **恒报"公开面漂移"**（与 API 面无关），并级联把 `docsync` 打成 `pass=294 fail=2`（`HC-4` 的门在交付平台**不可用**）。**修法**：比对前把两侧行尾统一为 LF 后比对**完整内容**（分节表头/计数一并比对，判别力与字节比对等价）—— 不依赖工作树行尾，Windows/Linux 行为一致。**根因旁证**：`--diff`/`--snapshot` 走 `item_lines`（仅条目行）故从未受此影响 —— GNU sed 读入时即剥离行尾 CR。
- **`docref` 的设计性空隙（`CO-4(v2.6)`）**：原判据的 `<end>` 由作者自选，`<end>` 之后**任意数量的文档提交不受检** —— 本版两处最严重文本错误（`CO-2`/`CO-3`）恰好全在该盲区。**修法**：增设"**记录终点必须覆盖到 HEAD**"判据（`<end>` 解析后须等于 `HEAD`），迫使 §4 随终态一起回填。

#### Changed
- **交付文档终态统一（`CO-3`）**：`v2.6开发交付__…md` 的 §1 状态声明 / §4 CI 行 / §5.1 的 G0-5 与 CI 行 / 文末纪律句 全部改为与 §2/§7 一致的终态表述（原"CI 未核验 / 远端待推送 / 本交付不合格"与终态结论互斥）。原则保留：**未能核验者不按通过计**。
- **三处同族漏刷与表述（`CO-6`/`CO-7`/`CO-12`）**：`README.md` 冻结面 checklist 第 8 条的基线名 `API_INVENTORY_v2.4.md` → **v2.5**；`port/stm32g474/README.md` 的 v2.6 行"板侧**未同步**" → "**已同步**（见实机记录 §14）"（同族 v2.5 行的"未上板"一并更正为"-已同步 + 走单见 §13"）；v2.6 交付文档 AC-18 证据行"实机记录、体积表本版零改动" → "**selftest 相关行**零改动"。
- **计划模板增至五项（`CO-11`）**：`docs/开发计划模板.md` §7 增第 ⑤ 项"**证据落点章节号 + 占位节清理**"（附 v2.6 反例：AC-1 指 §12、证据在 §13）；`移植stm32实机记录.md` §12 标题处标注"**已由 §13 承接**"；`README.md` checklist 第 10 条同步为"五项"。
- **docsync 增 4 条同族门（`CO-6`/`CO-7`/`CO-11` 的机制化）**：① README 冻结基线名 与 `tools/apidump.sh` 默认基线**交叉核对**；② `port/stm32g474/README.md` 不得残留"未同步"；③ 计划模板须含"证据落点章节号"；④ README 与模板的项数一致（五项）。docsync 断言 **296 → `301`**。
- **耗时读数更正（`CO-13`）**：见上 `tools/apidump.sh` 条（删除不可复现的"21s"）。

## [2.5.0] — 2026-09-19 · **v2.5 — Modbus 主站补齐与日志规格解析加固**


冻结契约下纯增量 MINOR（`apidump --diff` 对 v2.4 基线: 新增 16 项 / 修改删除 0 项）。

### Added
- **`et_modbus_master`**（protocol/，第 34 模块，25 例）：**Modbus RTU 主站**——0x03/0x04 读、0x06/0x10 写、**应答超时重发**（重发字节与首帧逐字节相同，含 CRC）、**异常码上报**（不重试）、**广播写不等应答**、**迟到/陈旧字节隔离**（非 BUSY 喂入即丢弃计数，新事务发起时清缓冲）；**复用从站协议常量**（`#include "et_modbus.h"`，功能码/异常码/数量边界/ADU 上限单一来源；沿 `et_xmodem_tx` 共享开关先例，不新增模块开关）；**职责边界**：只做**单事务**状态机，不含多从站轮询表/调度器/内部时基（`now_ms` 由调用方注入，超时阈值由调用方按波特率换算）；`rxcap`/`txcap` 硬底线 `ET_MODBUS_ADU_MAX`。API_GUIDE 5.8（状态机/四步流程/超时换算/重发与隔离语义/统计口径）+ 11.14 配方（**多从站轮询与 et_sched 协作**，显式声明库不提供调度器）。
- **`et_log` 规格解析加固（缺陷清偿 ★）**：旧实现对不支持的规格（`%02x`/`%.*s`/`%04u`）**原样回显字面量并错位消费后续实参**（v2.4 板上已咬人）。新实现按 C 语义**完整解析**标志{- 0 + 空格 #} + 十进制域宽（含 `*`）+ 精度（`.N`/`.*`）+ 长度修饰{h hh l ll z}，实参消费与 C 一致；**域宽/精度上限** `ET_LOG_FIELD_MAX`（默认 255，防刷屏）；已知不支持规格（`%o`/浮点族/`%n`/`%lc`/`%ls`）**按 C 消费实参并输出文档固定可见占位** `<?转换字符>`（`%n` 禁写内存），未知转换字符输出占位且**不消费**（头注与 API_GUIDE 明示）。
- **配方载体 #5**：`examples/ex_modbus_master.c`（内置**确定性模拟从站**，请求/应答/重发帧**逐字节**打印并断言：正常流/丢首应答驱动超时重发/异常/广播/迟到不污染/统计终值）进 `make ex`（四例 → **五例**）。
- **PC 从站仿真器**：`tools/modbus_master.py --slave`（`--slave-drop N` 丢包注入 / `--slave-exc` 异常注入 / `--slave-regs` 初值 / `--slave-frames` 限帧）——板侧主站走单**不再依赖第二台设备**；`--selftest` 扩至 29 项（含仿真器注入路径与请求帧提取），**跨实现对拍**：C 库组帧字节 == Python 组帧字节（`11 03 00 00 00 04 46 99` 等 5 组）。
- **评审记录载体（治理，P0-4）**：`docs/评审记录模板.md`（CO 编号规则 / 处置四选项 纳入-暂缓-拒绝-已关闭 / 触发条件字段 / 与计划双向互引 / 必须逐条不留白）+ README checklist 第 9 条 + docsync 自指断言（当前版交付文档须引用评审记录或声明其缺失）——关闭"本仓十五版无评审记录、遗留项靠人工回收"的缺口。
- bench v2.5 行：`modbus_master 请求组帧` 60.0 ns/op、`一轮事务(组帧+解析+终态)` 175.0 ns/op（本机负载波动，见 bench 文档抖动说明）。
- **ASan 门**：`make test-asan`（`-fsanitize=address`）+ CI 常设步骤——et_log 域宽上限用例的**越界机器证据**（AC-8）。

### Changed
- 版本 2.4.0 → **2.5.0**（`0x020400`→`0x020500`）；用例 432 → **474**（×双几何，1K 变体 475；log 7→24、新增 modbus_master 25）。
- **apidump 基线滚动**：`docs/API_INVENTORY_v2.4.md` 归档（417 项比较单元），默认基线切至 v2.4，v2.2/v2.3 归档只读保留。
- **CO-8 范式修正（文档+示例守护）**：API_GUIDE 5.7 示例循环改为 `mb_flush()` 覆盖**两个 flush 点**（feed 快路径 + tick 静默路径），正文明确"应答有两条产生路径"；`ex_modbus_slave` 增 tick 路径应答断言（v2.4 板上漏 flush 的坑变成 CI 守护的断言）。
- **调用点清理**：删除 g474/f103 demo 各 24 行手工拼补零绕行代码，改用 `%04u-%02u-%02u %02u:%02u:%02u`。
- **候选池复评落档（P0-2）**：`docs/v3-candidates.md` 增"v2.5 复评记录"节（biquad **维持排队**，下次触发=具体频响指标；32 位寄存器组合封装**拒绝（库级）**；主站单事务边界）。
- 数字回刷：README 特性表/结构树/用例数/examples 数、architecture 选型表与数据流、API_GUIDE 5.7/5.8/8.1/11.13/11.14、bench、port 体积表、docsync。

### Fixed
- **既有测试缺陷（v2.5 新增 ASan 门抓到）**：`test/test_modbus.c` 的 `mb_read_holding_normal()` 把 13 字节应答写进 `uint8_t d[8]`（`d[9..12]` 越界，ASan: stack-buffer-overflow）—— 自 v2.4 起存在，**非 ASan 构建不可见**。已修 `d[16]`。
- **`et_log` 的 `va_list` 链跨 ABI 缺陷（v2.5 首轮 CI 抓到并修复）**：`parse_spec`/`get_signed`/`get_unsigned`
  声明为 `va_list *ap`，而 `vformat` 形参是 `va_list ap`；x86-64 SysV 的 `va_list` 是**数组类型**（`__va_list_tag[1]`），
  形参退化后 `&ap` 得到 `__va_list_tag **` → `va_arg` 走错间接层（ubuntu 侧表现为**垃圾输出 + 段错误**）。
  **MinGW 的 `va_list` 是 `char*`，该错配不报警**，故本机全绿而 CI 红。修复：全链统一以 `va_list *` 贯穿。
  **持久门**：Makefile `CFLAGS` 增 `-Werror=incompatible-pointer-types`（Linux 侧硬失败，MinGW 侧零影响）。
- **et_log 实参错位（静默错误）**：`ET_LOGI("p","addr=0x%02x (kv regs %u..%u)", 17u, 1000u, 1003u)` 曾输出 `addr=0x%02x (kv regs 17..1000)`——字面回显 + 实参整体前移一格；`"n=%zu tail=%u"` 曾输出 `n=%zu tail=7`；`posix_demo` 的 `%.*s` 同形。三类形态均已在 `test/test_log.c` 以**哨兵实参 + host snprintf 逐字节对拍**守护（`log.misalign_regress`）。

- **计划缺陷（已实测证伪，如实记录）**：计划 §6 的「快照 tag 无副作用」**不成立** —— `release.yml` 触发条件为
  `tags: [v*]`，`v2.5-start` 匹配之，推送后**触发了 Release workflow**（run 35378057777，指向 v2.4 提交）。
  已立即 `gh run cancel` 取消（两 job 均 X，**未产生任何 Release**）。建议 v2.6 计划把快照 tag 改为不带 `v` 前缀
  （`snap-<ver>-start`）；详见交付文档 D-9 / N-8。

### 挂账（如实声明，见交付文档 §5）
- **板上真机走单未执行**：执行期 G474 板未接入本机（无 ST-Link、无 CH343 USB-TTL，仅有 WCH-Link），
  HC-10 的板上逐字节证据**未取得** → P2-3/P2-4 与 AC-21/AC-22 挂账（沿计划附 B "板档期"对策，不伪造结论）。
  库内主站/日志证据以 host 单测 + 五例 `make ex` + 跨实现对拍为准；板侧仅完成 `Core/et` 同步与交叉编译（0 warning）。
- **未打 `v2.5` 发版 tag**（有意为之）：计划 M5 末步为 `tag v2.5`，但 **HC-10 未满足**（板上逐字节证据缺失），
  按交付规约本交付不合格 → 不发布（tag 会触发 `release.yml` 出 Release 四附件）。库内成果已全部在 `origin/main`
  （`e44553c`，CI 七 job 全绿），板侧走单完成后打 tag 即发布。
- **selftest 套件维持 20**（未扩至 21）：与板档期冲突（扩套件需板上复验），按 HC-9 显式声明维持 20，记 v2.6 候选。

## [2.4.0] — 2026-09-12 · **v2.4 — Modbus 从站落地与板侧回归**

冻结契约下纯增量 MINOR（`apidump --diff` 对 v2.3 基线: 新增 22 项 / 修改删除 0 项）。

### Added
- **`et_modbus`**（protocol/，第 33 模块，23 例）：**Modbus RTU 从站**——0x03/0x04 读保持/输入寄存器、0x06/0x10 写单/多寄存器，异常码 0x01/0x02/0x03，广播写执行不应答、广播读忽略；**双路径帧判定**（已知功能码长度域快路径 + 帧间静默 3.5 字符兜底：未知功能码/畸形帧/残帧），`silence_ms` 按波特率换算由调用方注入（不内部取时基）；`rxbuf`/`txbuf` 双缓冲（**独立 TX 缓冲 = 粘包安全**：应答构建不覆盖后续帧），`feed` 产生一应答即停、`feed(NULL,0)` 排空粘包；CRC16-MODBUS 复用（查表加速），线上**低字节在前**；数量边界 125/123、ADU 256；零分配、🏠MAIN。API_GUIDE 5.7（协议表/API/静默换算/边界）+ 11.13 配方（**kv 参数暴露为保持寄存器**）。
- **配方载体 #4**：`examples/ex_modbus_slave.c`（脚本化主站帧 → 从站应答**逐字节比对**：正常流/三类异常/CRC 坏/地址不符/广播/分片/粘包/静默重同步全路径）进 `make ex`；**`tools/modbus_master.py`**（`--selftest` 内置从站仿真器回环自测 + 串口 `--read/--write/--write-multi/--raw/--addr/--port`）——板侧走单不依赖串口助手；工具自测首跑即抓出三处自身 bug（寄存器字节序取反/自检载荷地址拼错/期望帧字节错），修后与 C 库组帧**逐字节一致**（跨实现对拍）。
- **biquad 立项判定（P0-2，不实现）**：`docs/v3-candidates.md` #8 记"**维持排队, v2.5 复评**"——判定依据（现有 IIR 需求已被 et_lpf1 覆盖 / 无真实陷波-高通用例 / 条件未触发不消耗版本容量）+ 复评触发信号（用户提交具体频响指标）+ 实现预案。
- bench v2.4 行：`modbus 0x03 读 4 寄存器` 115 ns/帧（8.7 M 帧/s，协议处理相对串口线速可忽略）。

### Changed
- 版本 2.3.0 → **2.4.0**（`0x020300`→`0x020400`）；用例 409 → **432**（×双几何，1K 变体 433）。
- **CubeMX 板侧同步 v2.4（跨 v2.2→v2.4）**：0 warning；Modbus 从站挂 **USART1 + 帧首字节分流**（bench 只有一路 USB-TTL，USART2 独立需第二路串口；偏差与理由记实机记录 §11）；真机走单：0x03 读、0x06/0x10 写、异常码、CRC 坏帧静默 + **kv 参数经 Modbus 读写的掉电验证**；升级链/SELFTEST 回归不破。
- 数字回刷：README 特性表/结构树/用例数、architecture 选型表与数据流、API_GUIDE 5.7/11.13、getting-started、bench、docsync。

## [2.3.0] — 2026-09-12 · **v2.3 — 配方可执行化与直方图**

冻结契约下纯增量 MINOR（`apidump --diff` 对 v2.2 基线: 新增 11 项 / 修改删除 0 项）。本版全部项无板上依赖（刻意选择）。

### Added
- **`et_hist`**（algorithm/，第 32 模块，11 例）：定容直方图 —— 闭区间 [lo,hi] 等宽分桶（下含上含）、under/over 越界计数不丢弃、percentile 桶内线性插值**粗估**（桶宽=1 精确；声明非精确分位数）、O(1) 入桶零分配；`ET_MODULE_HIST`/`ET_HIST_BIN_MAX` 开关；与 et_stats（点估计）、et_sched_task_stats（last/max）互补的可观测性收尾。API_GUIDE 3.6 + 11.12 任务耗时分布诊断配方。
- **配方可执行化（★ P2）**：API_GUIDE 11.10/11.11 与升级流程落地为三个**自检式示例**——`examples/ex_pid_loop.c`（闭环整定 host 版，收敛值与 host 定点仿真逐值一致 + stats 判稳 + hist 控制量分布）、`ex_kv_backup.c`（kv_iter 导出 → xmodem 回环 → 解帧重灌 → 逐 key 比对）、`ex_upgrade_flow.c`（xmodem 写槽 → verify/stage → 模拟重启 → confirm 与超次回滚双路径）；`make ex` 一键全跑、CI 常设、任一 FAIL 即红——**配方正确性由 CI 守护而非由文档维护者保证**；示例只用公开 API（API 升级即编译错，证据不腐化）。API_GUIDE 三处"可执行载体"互链。
- **`docs/architecture.md`（P3）**：分层图 + 两条典型数据流 + **模块选型表**（按场景查 32 模块）+ 验证金字塔；与 getting-started 分工（architecture 管是什么/怎么选，getting-started 管怎么跑起来）。
- **apidump 基线滚动（P0-1）**：`docs/API_INVENTORY_v2.2.md` 归档（384 项比较单元），默认基线切至 v2.2，v2.1 归档只读保留。
- bench v2.3 行：`hist push` 2.0、`hist percentile` 10.0 ns/op（O(1) 入桶 + 桶内插值）。
- **v2.4 候选评估（P4-2）**：定点 Q15 双二阶（biquad）入 `docs/v3-candidates.md` #8（只评估立项条件，不实现）。

### Changed
- 版本 2.2.0 → **2.3.0**（`0x020200`→`0x020300`）；用例 398 → **409**（× 双几何，1K 变体 410）；CI unit-tests job 增设 `make ex` 步骤；selftest 维持 20 套件（沿 v2.2 决议不为扩充而扩充）。

## [2.2.0] — 2026-09-12 · **v2.2 — 控制上板与诊断增强**

冻结契约下纯增量 MINOR（`apidump --diff` 对 v2.1 基线: 新增 10 项 / 修改删除 0 项; `et_task_t` 字段追加走附则人工登记首次实战）。

### Added
- **`et_medfilt`**（algorithm/，第 31 模块，10 例）：奇数窗（3~`ET_MEDFILT_WIN_MAX`，默认 15）中值滤波 —— 脉冲尖峰抑制（单点尖峰不影响输出，窗内尖峰 ≤ (win-1)/2 时中值不动）、窗满前"下中位"语义文档化、小窗插入排序零分配；API_GUIDE 3.5 含与 movavg 取舍对比表 + `medfilt → lpf1` 两级配方。
- **`et_sched` 任务耗时统计（P4 增量 API）**：`et_sched_task_stats` / `et_sched_task_stats_reset`，poll_once 内注入前后时基差（不引入新时基依赖，亚毫秒任务报 0）；**`et_task_t` 尾部追加 `last_ms`/`max_ms` 字段 = API_STABILITY §5.1 登记区第一条**（破坏半径：调用方重编译即兼容，`--diff` 保持绿）。6 例。
- **板上闭环 demo（P2，CubeMX 板侧）**：`AT+PIDSET/PIDRUN/PIDOUT` 命令组 + 模拟被控对象 `y += k(u−y)`（10ms 节拍）；真机三组走单（保守/激进/积分钳位对照）与 host 同款定点公式仿真**逐值一致**（A: 496/0%/1110ms；B: 610/22%；C: imax=50 稳态钳在 410）——记录入 `移植stm32实机记录.md` §10，作为 API_GUIDE 11.10 的板上实例。
- **apidump 基线滚动（P0-1）**：`docs/API_INVENTORY_v2.1.md` 归档，`--diff` 默认基线切至最近已发布 MINOR（规则入 API_STABILITY §5），v2.0 归档只读保留。
- **kv 参数备份/恢复配方（P5-1，纯文档）**：`et_kv_iter` + `et_bytes_*` + `et_xmodem_tx` 组合的导出/恢复流程，API_GUIDE 11.11。
- **CRC16-MODBUS 查表（P5-3）**：`ET_CRC_TABLE` 第三表（512B），bench 142.9→500.0 MB/s；表路径与位算法 200 例随机对拍 + 标准向量双验证。
- bench v2.2 行：`medfilt push` 11.0、`sched poll_once` 5.0 ns/op。

### Changed
- **selftest 17→20 套件（P1，数字回刷全链）**：pid/stats/bytes 纯逻辑冒烟进 `et_selftest`（host/板上同套件），smoke.sh 断言、README、API_GUIDE 8.4、实机记录同步回刷；板上 `AT+SELFTEST` 20/20 实证。
- 版本 2.1.0 → **2.2.0**（`0x020100`→`0x020200`）；用例 382 → **398**（×双几何，1K 变体 399）；checklist 第 5 条交付文档命名放宽为"全角冒号或 `__` 均可"（v2.0 起实际惯例，v2.1 遗留①）。
- CubeMX 板侧 v2.2：FLASH 32100 → **36088 B**（pid/stats/bytes 被 demo 调用链入 + selftest 扩充 + PID 命令组，预期激活）。

## [2.1.0] — 2026-09-11 · **v2.1 — 定点控制与治理收口**

冻结后首个 MINOR（**API freeze 机检: 新增 27 项 / 修改删除 0 项**,纯追加实证）。

### Added
- **et_pid**（algorithm/，第 28 模块，18 例）：位置式定点 PID —— Q15 增益（kp 无量纲 / ki 1/s / kd s）、int64 饱和中间量（无有符号溢出 UB）、积分限幅抗饱和（钳位法，不做反算回灌）、`d_on_measure` 微分作用于测量值（推荐默认开，免除设定值跳变冲击）、`dt_ms` 由调用方给（不内部取时基）；非 ISR-safe 单上下文标注。整定配方入 API_GUIDE 11.10（`et_lpf1 → et_pid → et_spwm` 三段式链路 + Ziegler-Nichols 定性 + 常见坑）。
- **et_stats**（algorithm/，第 29 模块，11 例）：Welford 整数增量流式统计（min/max/均值/方差 Q10），不累加 Σx²（避免大数相减失真与溢出）；与 et_pid 联动判稳/记录超调；INT32_MIN/MAX 输入饱和不 UB。
- **et_bytes**（protocol/，第 30 模块，8 例）：BE/LE u16/u32 打包解包；边界检查即唯一面（越界/回绕拒绝且不读不写）。
- **apidump `--snapshot`/`--diff` 冻结基线机检（P0-1）**：`docs/API_INVENTORY_v2.0.md` 归档 v2.0 基线；`--diff` 纯新增=绿、签名删改/宏值变更=红、`ET_VERSION*` 元数据宏忽略；规则入 API_STABILITY 附则（结构体字段追加 apidump 不可见，须人工登记）。
- **docsync 自指断言（P0-2）**：当前版本交付文档须声明"本版 docsync N/N"，脚本与实测断言数对账，写错即红——关闭 v2.0 验收发现的自指盲区（同族第三例终局）。
- **`docs/getting-started.md` 端到端教程（P3-2）**：裁剪 → host 单测 → `port/_template` 移植 → 串口 shell → selftest → 闭环示例 → 提交自查，一步一命令。
- bench v2.1 行：`pid step` 15.0 ns/op、`stats push` 6.0 ns/op（同机中位数，环境注记同 v1.7）。

### Changed
- 版本 2.0.0 → **2.1.0**（`ET_VERSION = 0x020100`）；用例 345 → **382**（× 双几何；1K 变体 383、shell Tab 形态同套件数）。
- **CubeMX 板侧同步 v2.1（跨 v1.9→v2.1 两版）**：`Core/et/` 全量重拷 + `diff -rq` 校验；零警告构建；新模块未被 demo 调用 → 链接器丢弃 59 个 section，FLASH **32100 B 与 v1.9 逐字节等值**（板端零变化）。板上复验：横幅 v2.1.0（0x20100）/ `AT+SELFTEST` **17/17** / `AT+SELFSTOR` kv+bootctl PASS / 升级链 `SIMUPGRADE 3` confirm + `SIMUPGRADE 4` self-check FAILED→ROLLBACK 双路径（记录入 `移植stm32实机记录.md` §9）。
- API_GUIDE 增 3.3/3.4/5.6 章节与 11.10 整定配方、配置表补三模块开关；README 特性表/用例数/checklist 第 8 条（冻结机检 + 自指断言）。

### Fixed
- `et_bytes.h` 补 `<stddef.h>`：板侧零警告构建暴露头文件不自足（host 侧因间接包含而未显形）——v2.1 板侧同步机制的价值实例。

## [2.0.0] — 2026-09-09 · **v2.0 — API freeze（契约冻结与工程单一来源）**

契约冻结与工程单一来源（里程碑版：API 冻结声明 + 构建单一来源 + size 门）。

### Added
- 构建/发布单一来源（P0-2）：CI/Release 的 ARM 交叉构建与 coverage 全部转调 `tools/docbuild.sh`/Makefile（过渡期新旧产物 sha256 一致）；`tools/sizecheck.sh` 体积表逐值门（P0-3，首跑即抓版本宏 -4B 漂移自证有效）。
- **API 冻结契约三件套（P1）**：`tools/apidump.sh` 公开面清单（175 函数/66 类型/96 宏，漂移即红——首跑抓到 ET_DEPRECATED 未登记，机制自证）+ `docs/API_STABILITY.md`（v2.0 起冻结、MINOR 只增、弃用流程）+ `ET_DEPRECATED` 宏（GCC attribute/非 GCC 空展开）；一致性审计 7 项，触碰签名者入 `docs/v3-candidates.md`（本版零破坏）。
- **`et_smap` 大小写折叠变体（P2 决议）**：`et_smap_put_ci/get_ci/del_ci`（ASCII 折叠入共用表/池，与敏感条目共表共存），4 例；通配终局关闭。
- **shell Tab 命令名补全（P2 决议）**：`ET_SHELL_TAB`（默认 0=行为零变化），大小写敏感前缀匹配、唯一补全/多候选列表/无匹配响铃，双形态矩阵 8 例（`make test-tab`，CI 常设）。

### Changed
- f103 port README 构建块升级为双构建 docbuild 单一来源（默认 + _selftest；Renode smoke 改消费 `_selftest.elf`）。
- 用例矩阵终值：345 例 × 双几何 + 1K 变体 346 + `make test-tab`(开启形态 8 例)；f103 体积终值 17836/26304、g474 18208 [数字回刷]。
- 单一来源纪律外溢两例(本版内发现并修)：CI YAML 步骤名裸冒号/块标量内顶格多行 printf(解析红)；apidump 在 UTF-8 locale 的 gawk 下对含 emoji 注释行正则漂移(吞声明)→ 脚本内锁 `LC_ALL=C` 字节模式跨环境一致。

### Added（P3/P4）
- Release 正文自动生成：release.yml 注入 CHANGELOG 对应版本节（无节即红，v1.7 遗留双源终结）。
- `port/_template/` 新平台移植脚手架：契约四件套骨架 + 几何宏清单 + `#error` 守卫样例 + 8 步接入 checklist（G474/F103 教训浓缩；模板编译/漏 -D 即败双验证）。
- bench `smap ci get` 行（11.0 ns/op，折叠 +3ns）；CI 上传 API 清单 artifact；GitHub Issue 模板（移植求助/Bug 复现）。

### Fixed / Changed / 挂账
- v1.9 三处滞后数字回刷（17716/26180→v2.0 实测 17712/26180/18084；docsync 121→122）。

## [1.9.0] — 2026-09-08

板上收口与字符串映射（三版板面挂账一次清偿 + et_smap + 文档可执行化）。

### Added
- **et_smap**（core/，第 27 模块，14 例）：定容字符串键映射 —— FNV-1a 32 位 + 内嵌键存储池拷入语义 + 哈希缓存/键长预比较；探测/墓碑/满表拒绝语义与 et_map 同规则（交叉引用不重复定义）；`ET_MODULE_SMAP`/`ET_SMAP_KEY_MAX` 开关；API_GUIDE 2.6 节 + 11.9 命令路由/配置表配方 + et_map 键 0 保留 FAQ（v1.9 决议：改键域需重设计状态编码，应用侧走 key+1 偏移）。
- **板上收口全套**（v1.6~v1.8 挂账清偿）：升级链真机走单 1~4（SIMUPGRADE 回滚/confirm / xmodem 真传 STAGED→CONFIRMED / IWDG 真超时复位+复位原因）、库化 selftest 板上记录（17/17 + SELFSTOR 实跑）、tickless+RX 中断唤醒实测 —— 记录回填 `移植stm32实机记录.md` §5；走单 5（LED 极性/按键补录）待用户目视确认。
- **`tools/pack_image.py`** ETBI 镜像打包（走单 3 配套，头布局注释与 et_bootctl 对齐）。
- **CI `docbuild` job + `tools/docbuild.sh`**：port README ```docbuild 定界构建块原样执行（含 objcopy），文档命令成为 CI 门；发布 checklist 第 7 条"数字回刷纪律"。

### Changed
- `ET_CRC_TABLE` 扩展至 **CRC32/IEEE 查表**（1KB 表驻只读段，同 SECTION 机制）：bench crc32 125→500 MB/s；表/位算法随机对拍逐字节一致。
- F103 demo selftest 调用全部 `#if ET_MODULE_SELFTEST` 守卫 + README 构建命令补 `storage/*.c`（P1-1：文档命令无宏原样可构建，实测 text 16852）；两裸机 demo 命令表计数改 sizeof 自适应。
- bench 增 map/smap 查找行（2 vs 8 ns/op，负载 0.62 质数 97 槽）。

### Fixed（走单实机暴露，5+3 处）
- demo 升级链：`verify_image/stage` 误传扇区号（应传槽序号 0/1，三 demo，v1.5 起该路径真机从未通过）；`SIMUPGRADE ver` 数字解析不归零；升级前缺 `abandon()`（状态机"confirm 后 stage 拒绝"规则）；`xm_sink` 无槽容量守卫（跨界写会踩邻接状态扇区）；xmodem 成功判定用 `total`（DONE 时已被 session_reset 清零，恒假）→ 改 DONE 动作，且 DONE 需回收尾 ACK（协议 EOT 二段确认）。
- port：`port_wdt_enable` 冷启动时序 —— 软件模式下 LSI 仅在 START 后起振，旧实现先等 PVU/RVU 再 START 必然 guard 超时（返回 false 却已写 START，狗按缺省周期跑一周期）→ 改 HAL_IWDG_Init 参考序 START→等同步→喂狗（F103/G474/CubeMX 三处）。
- 库：`et_selftest` bootctl 套件强制干净起步（init 后 `abandon()`，此前跑于真实升级之后会连锁误报 6 断言）。
- CubeMX demo：WDTEST 后 IWDG 不可停 → 开机检测 IWDGRSTF 自愈持续喂狗（板不再入复位循环）。

### 治理
- docsync：CHANGELOG 断言由写死 1.7.0 改为**版本链动态全覆盖**（v1.8 条目缺失即此类漂移实例，本版补齐 1.8.0/1.9.0）；数字回刷 v1.8 三处滞后（25200/17、87 全表）。

### 挂账
- ~~走单 5：LED PC0 极性目视 + PD15 短按/长按补录~~ ✅ 2026-09-09 上板人现场确认：PC0 高有效、短按/长按全对——板面挂账清零。

## [1.8.0] — 2026-09-07

定容映射与互传对称化（容器补齐 + MCU 能收能发）。

### Added
- **et_map**（core/，第 25 模块，13 例）：定容开放寻址哈希表 —— u32 键、线性探测+probe_limit 超限拒绝、墓碑删除复用、foreach 遍历安全；键 0/0xFFFFFFFF 保留。
- **et_xmodem_tx**（protocol/，第 26 模块，14 例）：XMODEM-CRC 发送器状态机（WAIT_START→DATA→EOT1→EOT2→done），与接收器共享协议常量/`et_xmodem_crc16`（单一事实来源）；tx→rx 内存回环双块型逐字节一致入常规回归。
- F103/G474 裸机 demo：**RX 中断 + et_ringbuf**（G474 教训闭环）、tickless 主循环（next_due 门控）、F103 `AT+SELFTEST` 接入 + 开机自跑（Renode smoke 断言载体）；smoke 断言扩至 selftest 17/17。

### Changed
- selftest xmodem 套件升级为 tx→rx 回环；docsync 87 断言（含交付文档覆盖率行循环断言）；双几何 326×2 全绿（1K 变体 327）。

### 挂账
- 三版板面收口（v1.6 走单 / v1.7 库化 selftest 记录 / v1.8 板面验证）→ tag v1.6/v1.7/v1.8 悬置（本版由 v1.9 P0 清偿）。

## [1.7.0] — 2026-09-06

板上自测与性能量化(验证金字塔封顶)。

### Added
- **et_selftest**(debug/,第 24 模块,`ET_MODULE_SELFTEST` 默认 0 可裁剪):板上自测组件 —— 套件注册表 + 结构化报告回调 + 动态注册槽 + 存储门控(kv/bootctl 破坏性套件默认 SKIP);17 内建套件(ringbuf/queue/mempool/list/filter/fsm/sched/event/stimer/crc/frame/softclock/wdt/atcmd+xmodem/kv/bootctl);host 侧同组件复跑(test_main 注册,板上/PC 结果可比对);框架自测 9 例。
- **host 基准** `tools/bench.c` + `make bench/bench-table` + `docs/bench.md`(固定迭代 + 5 轮中位数 + 防 DCE volatile 汇聚 + 环境注记强制)。
- **CHANGELOG.md**(本文档)。
- tickless 接入配方与 G474 工程 `AT+SELFTEST` 库化(私有实现退役)。

### Changed
- Renode G4 由挂账转**政策关闭**(不排期:G474 真机已承担 G4 平台验证职责)。

### 挂账
- v1.6 升级链走单 5 条回填 + tag v1.6 补打(顺序收口,前置 v1.7 tag)。

## [1.6.0] — 2026-09-06

G4 入库、双几何回归与 tickless。

### Added
- `port/stm32g474/` 裸机移植(144MHz 时钟树/双 bank 2KB 页参数区/IWDG 32kHz/#error 几何守卫)+ `stm32g474_demo`。
- G474 真机验证记录(kv 重启计数跨上电递增 + AT+SELFTEST 13/13)。
- **双几何回归制度**:CI `cross-stm32g474` job(含漏配负向验证)+ `make test-g4` + docsync 断言。
- **tickless 增量 API**:`et_sched_next_due()` / `et_stimer_next_due()` / `PORT_TICK_WAIT_FOREVER`(头文件注明 WFI/RX 唤醒配对约束)。
- `ET_KV_VAL_MAX` 公开容量宏(容量经 API 计算,用例不得硬编码)。
- `tools/xmodem_send.py` + `tools/xmodem_host_recv.c`(升级链走单工具,host 双块型端到端闭环)。

### Changed
- **8B 槽适配**(G4 flash 64 位双字单次编程约束):et_kv 页头拆双字两阶段提交 + 记录槽 ALIGN8;et_bootctl 状态头 12B→16B —— 语义不变,host 291×2 几何全绿。

### Fixed
- G474 实机暴露:UART RX 轮询在 WFI 空闲下因 RDR 单字节深度溢出丢字节 → 中断 + et_ringbuf(见实机记录问题 1)。

## [1.5.0] — 2026-09-05

安全升级与运行防护。

### Added
- **et_bootctl**(storage/,第 22 模块):32B 版本化镜像头 + A/B 试运行/确认/回滚状态机,append-only 状态记录 + 0→1 位写,任意断电只丢最后一步;24 例含掉电矩阵。
- **et_wdt**(sys/,第 23 模块):port 契约第二次演进(wdt 三件套);enable 下限 = ERASE_MS_MAX×2;F103 IWDG 寄存器直驱;et_wdt_guard 阻塞段保护;10 例。
- shell 可选历史 `ET_SHELL_HISTORY_N`(上/下键回放 + 行内编辑);6 例双变体。
- demo 全链路串联:shell → AT+UPGRADE(xmodem)/AT+SIMUPGRADE → 开机引导决策(超次回滚)。
- docsync.sh 文档同步断言(42 条)入驻 CI;Release 附件 SHA256SUMS。

## [1.4.0] — 2026-09

命令交互与传输生态。

### Added
- **et_xmodem**(protocol/,第 20 模块):XMODEM-CRC 接收器,超时注入式设计,ET_XM_1K 可选大块;17 例对端行为矩阵。
- **et_shell**(debug/,第 21 模块):atcmd 之上的薄交互壳,回显/退格擦写/help 自动生成;15 例。
- **et_kv_iter** 增量 API:只读枚举活跃页有效 key(快照语义 + tombstone 跳过);7 例。

### Fixed
- 发布 checklist 强化:量化声明必须附复现命令 + 环境注记。

## [1.3.0] — 2026-09

平台实测闭环与 et_fsm。

### Added
- **et_fsm**(algorithm/,第 19 模块):表驱动状态机,const 迁移表可驻 flash、guard 回退链、零分配;15 例,行覆盖 100%。
- Renode 仿真闭环:headless 跑通 F103 demo 全功能并断言串口关键日志;`renode-smoke` 进驻 CI;release.yml 拆 verify→release 两段(验证门不过不出版本)。
- 开发者发布 checklist(每项声明附复现命令)。

### Fixed
- **仿真首跑揪出 v1.2 遗留 F103 flash 驱动严重 bug**(host 191 用例全绿未暴露)。

## [1.2.0] — 2026-09

et_kv 存储与实测补录。

### Added
- **et_kv**(storage/,第 18 模块):双扇区乒乓键值掉电存储,MOVING→COMMITTED 两阶段页生效,页满自动压实,断电恢复矩阵全绿。
- port 契约演进(纯增量):flash 三件套(read/write/erase_sector)+ 几何宏 + 语义约定(4B 对齐/1→0 写/短写上报)。
- F103 寄存器级 flash 驱动(PM0056);demo 重启计数 + 软时钟持久化。
- CI 覆盖率门槛(行 ≥85%)+ tag 发布工作流。

## [1.1.0] — 2026-09

平台移植与新模块。

### Added
- `core/et_list`(侵入式双向链表)/`algorithm/et_filter`(定点滤波器组)/`drivers/et_spwm`(软件 PWM)三新模块;16 模块 / 150 用例。
- **STM32F103 裸机移植**(寄存器级,零 HAL):port 层 + 启动代码 + 链接脚本 + demo。
- GitHub Actions CI(ubuntu+windows 单测矩阵 / gcovr 覆盖率 / ARM 交叉编译)。

### Fixed
- 3 处 v1.0 遗留可移植性问题(未改任何既有 API 语义)。

## [1.0.0] — 2026-09

初版:13 模块 / 103 用例,零动态内存、多实例句柄化、分层单向依赖、PC 可全量单测(core: ringbuf/queue/mempool;algorithm/filter;sys: stimer/sched/event;protocol: crc/frame/atcmd;drivers: key/led;debug: log/assert)。

[Unreleased]: https://github.com/azura112/Embedded_Tools
[1.7.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.7
[1.6.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.6
[1.5.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.5
[1.4.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.4
[1.3.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.3
[1.2.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.2
[1.1.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.1
[1.0.0]: https://github.com/azura112/Embedded_Tools/releases/tag/v1.0
