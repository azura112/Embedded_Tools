# et_kv 技术验证提案：port flash 契约演进（v1.2 候选）

> 分支：`exp/kv`（技术验证期**不进 main**）｜ 起草：2026-09-02 ｜ 状态：待评审
> 背景：v1.1 计划 P2-4 —— et_kv 是 port.h 的**首次契约演进**，按纪律须先立契约小节评审，再动手实现。

## 1. 目标

为 flash 参数存储（键值对、掉电保持）扩展 port 契约。复用既有不变式：零动态内存、ISR-safe 显式标注、et_crc32 校验复用。

## 2. 契约扩展提案（port.h 追加）

```c
/* ---- flash 分区访问(按扇区, 仅 et_kv 使用) ---- */

/* 扇区数与扇区大小由平台静态声明(供编译期算布局) */
#define PORT_FLASH_SECTOR_SIZE      1024u        /* 平台定制, 例: F103 中容量 1K */
#define PORT_FLASH_SECTOR_COUNT     16u          /* 分配给参数区的扇区数 */

/* 读: addr 为参数区基地址起算的偏移, 原子性无要求 */
bool port_flash_read(uint32_t offset, void *buf, uint32_t len);

/* 写: 只能 0→1 位写, len 须为编程粒度(≥4B)对齐; 返回实际写入 */
uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len);

/* 整扇区擦除: 1→0xFF, 耗时操作(ms 级), 仅限 🏠MAIN 调用 */
bool port_flash_erase_sector(uint32_t sector_index);
```

### 评审要点（动手前必须逐条结论）

1. **写入粒度**：F103 为 16/32/64 位页写 + 16 位擦除粒度；提案按"4B 对齐"是否统一各平台？
2. **擦除耗时与看门狗**：erase 毫秒级阻塞，契约须声明"调用方负责喂狗/停调度"还是 port 内部处理？
3. **中断期间禁写**：flash 操作期间禁止取指同 bank 代码（F103 中断向量在 flash）→ 契约要求 erase/write 仅 🏠MAIN 且包临界区？还是 port 自行关中断？
4. **双扇区乒乓与磨损**：追加写 + 双扇区乒乓 + CRC32 校验 + 序列号仲裁（掉电恢复取最新有效页）；是否需要磨损均衡 v1（仅乒乓两扇区，够参数存储场景）？
5. **契约冻结点**：本节合并进 API_GUIDE 第 8 章后才允许 et_kv 动工。

## 3. et_kv 模块设计草案（评审后实现）

```
[页头: magic|seq|crc32(页头)] [记录: key|len|crc32(payload)|payload] ...
```

- 写入：只追加新版本记录，读侧取 key 的最新有效记录；页满则乒乓扇区+搬迁存活记录；
- 初始化：扫描两扇区，选 magic/crc/seq 均合法且 seq 最大的页；坏页标记弃用；
- API 预览：`et_kv_init(扇区基址布局)` / `et_kv_get(key, buf, cap)` / `et_kv_set(key, buf, len)` / `et_kv_commit()`；
- 🏠MAIN 专用（非 ISR-safe，与 mempool 同级标注）。

## 4. 决策记录

- v1.1 评审结论：P0~P3 已按期完成，但 kv 触发契约演进 + 工作量最大，按计划"否则定版 v1.2 首个特性"条款，**定版 v1.2 首个特性**；
- 本分支保留契约提案与设计草案，v1.2 开工时直接引用评审。

## 5. v1.2 书面评审决议（2026-09-02，契约冻结）

按 v1.2 计划 P0 门槛对第 2 节 5 个评审要点逐条决议；默认决议全部确认，无异议项。

| # | 要点 | 决议 | 落地 |
|---|---|---|---|
| 1 | 写入粒度 | **确认**：统一 4B 对齐；F103 半字编程为其超集（port 内部按 4B 循环半字编程）；host 模拟无粒度限制 | port.h 契约注释 |
| 2 | 擦除耗时与看门狗 | **确认**：调用方负责喂狗/停调度；port 定义 `PORT_FLASH_ERASE_MS_MAX` 供看门狗超时配置参考 | port.h 契约常量 |
| 3 | 擦写期间中断 | **确认**：erase/write 仅限 🏠MAIN，port 内部以 `PORT_CRITICAL_*` 包住完整擦写序列 | port.h 契约注释 + F103 驱动实现 |
| 4 | 乒乓与磨损均衡 | **确认**：v1.2 双扇区固定乒乓，不做均衡算法；寿命估算写入 API_GUIDE | API_GUIDE §8.2 |
| 5 | 契约冻结点 | **确认**：本节决议后契约合入 API_GUIDE 第 8 章，随后 et_kv 动工 | M1 合入顺序 |

**评审新增点 — 删除语义**：**确认**增加 `et_kv_del(key)`，写入 tombstone 记录，读侧跳过，搬迁时不搬迁死记录（含 tombstone——源页切换后旧页不再被读，tombstone 无保留价值，可安全丢弃）。

**评审新增决议（设计细化，为实现正确性必需）**：

| # | 议题 | 决议 |
|---|---|---|
| A | 搬迁中途掉电的对页误选风险 | 页头增设 `state` 字段：写页头时保持 0xFFFFFFFF（MOVING），记录全部搬完后编程写 0x00000000（COMMITTED，1→0 位写合法）。页有效性 = magic + hdr_crc + state==COMMITTED 三条件；init 只认 COMMITTED 页，RECEIVING 页弃用待擦，源页全程不破坏 |
| B | 追加写中途掉电的"脏尾" | init 扫描遇到不可解析记录（key/len 字段非法）即认定写入区结束并触发**自动搬迁**修复（把有效记录搬去对页）；可解析但 CRC 失败的记录仅跳过（位翻转场景，不中断后续记录可见性） |
| C | key 编码与哨兵冲突 | 用户 key ∈ [1, 0x7FFE]；tombstone = key \| 0x8000；0xFFFF（= tombstone(0x7FFF)）与"未写哨兵"冲突，故 0x7FFF 保留不可用（文档声明） |
| D | API 补充 | 在计划基线（init/get/set/del/commit/stats）之上增加 `et_kv_format()`（显式格式化，用于 init 失败后的现场恢复）与 `et_kv_size()`（key 值长度查询，配合 get 缓冲分配）；commit() 语义定稿为"手动压实"（set 本身即时持久化，无丢数据窗口） |
| E | 擦写计数的掉电保持 | 页头不扩展（位写约束下计数递增需额外擦写，复杂度不成正比）；`et_kv_stats()` 的擦写计数为**本次上电累计**，跨掉电的等效磨损可用 seq 近似（每次页切换 +1），文档注明 |

