# API 稳定性契约 (v2.0 起生效)

> 公开面清单: **[API_INVENTORY.md](API_INVENTORY.md)**(`tools/apidump.sh` 生成,docsync 断言一致);
> 本文件是承诺与流程,清单是事实。语义/用法示例见 [API_GUIDE.md](API_GUIDE.md)。

## 1. 冻结声明

自 **v2.0.0** 起,`API_INVENTORY.md` 所列全部公开面(175 函数 / 66 类型 / 96 配置宏,
含 `port.h` 契约)**冻结**:

- **MINOR 版本(2.x)只允许追加**:新增函数/类型/宏;不改动既有签名、语义、上下文标注;
- **破坏性变更唯一入口 = 弃用流程**(§3),且**仅发生在 MAJOR**(下一个 MAJOR 才会是 3.0 窗口);
- `port.h` 平台契约同受冻结约束:新增 port 钩子以"默认实现/弱符号 + 版本宏门(`ET_VERSION >=`)"方式追加;
- 不变式(零动态内存、多实例句柄化、core/algorithm 零硬件依赖、ISR-safe 显式标注)是承诺的
  组成部分,不随版本协商。

`et_config.h` 的 `ET_VERSION` 供条件编译:`#if ET_VERSION >= 0x020100` 安全使用 2.1+ 追加项。

## 2. 一致性审计结论(v2.0 P1-3,评审时间盒内)

四类逐项:返回风格 / 参数序 / 命名规约 / 上下文标注。**触碰既有签名的一律记入
[v3-candidates.md](v3-candidates.md),本版不改**(DoD:冻结即本版目的)。

| # | 发现 | 类别 | 处置 |
|---|---|---|---|
| 1 | 回调 `user` 位置两种流派:`et_xm_sink_fn/et_led_write_fn/et_smap_visit_fn`(首参)vs `et_atcmd_fn/et_map_visit_fn/et_list_visit_fn`(末参) | 参数序 | **v3 候选**(统一 user 位需破坏签名) |
| 2 | `et_map_foreach` 回调拿可变槽指针(`et_map_slot_t *`),`et_smap_foreach` 回调拿 `(key, *val)` —— 语义各宜但形状不一 | 参数序 | **v3 候选**;本版已在两 API 注释处显式声明差异(交叉引用),文档面已一致 |
| 3 | 句柄首参规则:全部状态型模块首参为句柄 ✓;无句柄 API 均为**有意设计**——全局注册表(`et_wdt_*/et_sched_*/et_stimer_*`)、纯函数(`et_crc*/et_mempool_bytes_needed/et_xmodem_crc16`) | 参数序 | **确认合规**,清单如实登记,不整改 |
| 4 | 返回风格:变更/谓词类 `bool`,计数类 `uint32_t`,纯计算类值返回 —— 抽查无违例 | 返回风格 | **无需处置** |
| 5 | 上下文标注缺失 5 头(`et_crc/et_frame/et_atcmd/et_led/et_assert`) | 标注 | **doc-only 已修**(v2.0 本提交补注,签名面零变化——apidump `--check` 实证) |
| 6 | 命名:`et_xxx_init/put/get/del/count/clear/foreach` 规约在 map/smap 对齐 ✓;`et_ringbuf_read/write` vs `et_queue_put/get` 动词不一(历史语义清楚) | 命名 | **v3 候选**(改名破坏面大,收益低);本版不动 |
| 7 | `et_stimer_poll/et_sched_poll_once` 同名概念两个动词 | 命名 | **v3 候选**,同注 |

审计方法:回调 typedef 全量扫描(20 个)+ 非句柄首参函数清单 + 标注 grep;
结论:公开面自 v2.0 可安全冻结,遗留不一致均已定性(文档/候选),不阻塞发布。

## 3. 弃用流程(破坏性变更唯一入口)

1. **标记**:在既有符号上挂 `ET_DEPRECATED`(et_config.h,GCC `__attribute__((deprecated))`,
   非 GCC 空展开)——签名不变,仅告警;
2. **观察期**:保留 **≥2 个 MINOR 版本**(替代 API 必须在标记同一版本可用);
3. **移除**:仅在下一个 MAJOR(3.x 窗口)删除,CHANGELOG 列迁移对照表;
4. 清单联动:标记与移除都要重新生成 `API_INVENTORY.md` 并过 docsync `--check`。

## 4. 机制(承诺如何被强制)

| 机制 | 作用 |
|---|---|
| `tools/apidump.sh --check`(docsync 调用) | 新增/变更公开符号未登记清单 → CI 红 |
| `tools/apidump.sh --diff`(v2.1) | 与冻结基线比对:**纯新增=绿; 签名删改/宏值变更=红** |
| docbuild 门(v1.9) | 文档构建命令可执行,README 即单一来源 |
| `tools/sizecheck.sh`(v2.0) | 体积表当前版本行与实测逐值比对(发布前本地跑) |
| CI 双交叉 + Renode smoke + 双几何回归 | 行为面回归 |
| CHANGELOG + 交付文档 + docsync 版本链 | 声明与实际对账 |

## 5. 附则:`apidump --diff` 判定规则 (v2.1)

"MINOR 只追加"的**机检**实现 —— 用 `tools/apidump.sh --diff [基线]`(默认基线
`docs/API_INVENTORY_v2.0.md`,由 `--snapshot` 在版本冻结点归档)比对当前头文件与冻结基线:

| 情形 | 判定 |
|---|---|
| 新增函数/类型/宏(只追加) | **OK**(计入"新增 N 项") |
| 删除任一清单条目 | **红**(违反只增契约) |
| 修改任一签名/宏值(表现为 1 删 + 1 增) | **红**——一切破坏性变更只能走 §3 弃用流程 |
| `ET_VERSION*` 版本元数据宏 | **忽略**(随版本必然变化,不属冻结面) |
| 结构体**字段追加** | apidump 只登记类型名,不可见 → **须在交付文档 diff 说明区人工登记**(理由 + 破坏半径评估) |

自证:`--diff` 对基线自身为 0/0 绿;对故意改签名行(1 增 1 删)变红(v2.1 M1 记录)。
v2.1 实绩:新增 27 项 / 修改删除 0 项 —— 冻结后首个 MINOR 的纯追加机检通过。

v2.0 候选关闭决议(smap 折叠/通配、u32 全键域、shell Tab)见 CHANGELOG 与 API_GUIDE FAQ,
本文件链接不重复正文。
