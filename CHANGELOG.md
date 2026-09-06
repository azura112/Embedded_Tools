# Changelog

Embedded_Tools 版本变更记录。格式沿 [Keep a Changelog](https://keepachangelog.com/);
每版条目自该版交付文档"重点概况"提炼,**随交付更新**(模板:Added/Changed/Fixed/挂账)。
版本路线全文见各版 `v<版本>开发计划/交付` 文档。

## [Unreleased]

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
