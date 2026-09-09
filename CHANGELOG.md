# Changelog

Embedded_Tools 版本变更记录。格式沿 [Keep a Changelog](https://keepachangelog.com/);
每版条目自该版交付文档"重点概况"提炼,**随交付更新**(模板:Added/Changed/Fixed/挂账)。
版本路线全文见各版 `v<版本>开发计划/交付` 文档。

## [Unreleased]

## [2.0.0] — 开发中（发布时定稿）· **v2.0 — API freeze**

契约冻结与工程单一来源（里程碑版：API 冻结声明 + 构建单一来源 + size 门）。

### Added
- 构建/发布单一来源（P0-2）：CI/Release 的 ARM 交叉构建与 coverage 全部转调 `tools/docbuild.sh`/Makefile（过渡期新旧产物 sha256 一致）；`tools/sizecheck.sh` 体积表逐值门（P0-3，首跑即抓版本宏 -4B 漂移自证有效）。
- **API 冻结契约三件套（P1）**：`tools/apidump.sh` 公开面清单（175 函数/66 类型/96 宏，漂移即红——首跑抓到 ET_DEPRECATED 未登记，机制自证）+ `docs/API_STABILITY.md`（v2.0 起冻结、MINOR 只增、弃用流程）+ `ET_DEPRECATED` 宏（GCC attribute/非 GCC 空展开）；一致性审计 7 项，触碰签名者入 `docs/v3-candidates.md`（本版零破坏）。
- **`et_smap` 大小写折叠变体（P2 决议）**：`et_smap_put_ci/get_ci/del_ci`（ASCII 折叠入共用表/池，与敏感条目共表共存），4 例；通配终局关闭。
- **shell Tab 命令名补全（P2 决议）**：`ET_SHELL_TAB`（默认 0=行为零变化），大小写敏感前缀匹配、唯一补全/多候选列表/无匹配响铃，双形态矩阵 8 例（`make test-tab`，CI 常设）。

### Changed
- f103 port README 构建块升级为双构建 docbuild 单一来源（默认 + _selftest；Renode smoke 改消费 `_selftest.elf`）。
- 用例矩阵终值：345 例 × 双几何 + 1K 变体 346 + `make test-tab`(开启形态 8 例)；f103 体积终值 17836/26304、g474 18208 [数字回刷]。

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
