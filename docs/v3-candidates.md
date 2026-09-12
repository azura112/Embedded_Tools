# v3.0 候选清单(v2.0 冻结审计遗留,破坏性窗口专用)

> 纪律:本清单是"触碰既有签名"项的**唯一去向**(v2.0 P1-3);攒到 v3 评审窗口
> 一次性处置。每项须给出破坏半径评估与迁移方案草案才可进 v3 计划。

## 来自 v2.0 API 冻结审计(docs/API_STABILITY.md §2)

| # | 项 | 破坏面 | 备注 |
|---|---|---|---|
| 1 | 回调 `user` 参数位置统一(现两派并存:首参派 sink/led/smap_visit/report 等 vs 末参派 atcmd/map_visit/list_visit/key_event 等) | 全部 20 个回调 typedef + 用户实现 | 建议统一 user **首参**(与句柄首参规约一致);迁移=机械替换 |
| 2 | `et_map_foreach` 回调形状与 `et_smap_foreach` 统一(槽指针 vs key+val 快照) | 2 个 visit 类型 | 若统一为快照+改值指针,`et_map` 的"改键置墓碑"删除路径需另给 `et_map_del_at` 类 API |
| 3 | `et_ringbuf_read/write` 与 `et_queue_put/get` 动词统一 | ringbuf 3 函数族 | 纯改名;评估收益低,默认**维持不改**,仅记录 |
| 4 | `et_stimer_poll` vs `et_sched_poll_once` 命名收敛 | 1 个改名 | 同 3 |

## 来自功能候选评估(v1.9 §5.4 / v2.0 P2 决议)

| # | 项 | 状态 |
|---|---|---|
| 5 | smap 通配/模式查找 | v2.0 决议:需求未证实,**关闭**;真需求出现再入表 |
| 6 | u32 全键域容器(键 0 可用) | v2.0 终局:由应用侧 `key+1` 偏移覆盖(FAQ),**关闭不立项** |
| 7 | shell 路径补全/多行编辑/历史搜索 | Tab 命令名补全 v2.0 已交付(`ET_SHELL_TAB`);其余**维持永久排除**,除非需求信号改变边界 |
| 8 | 定点 Q15 双二阶(biquad)滤波器 | **v2.4 候选**(v2.3 P4-2 评估): 立项条件 = 出现真实 IIR 型频响需求(陷波/峰值 EQ/高通截止)且 medfilt+lpf1 两级链不够用; 直接 IIA 转置型 + int64 累加 + 系数 Q15, 语义与 et_lpf1 同族; 不做浮点变体 |
