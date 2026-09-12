# host 基准 (docs/bench)

> 度量工具:`tools/bench.c`(固定迭代 + 5 轮取中位数,防 DCE volatile 汇聚)。
> **声明:数字不具跨机器可比性**,仅用于同机版本间回归对比与量级判断;
> 复测请附完整环境注记,无注记数字不入本文档。

## 环境注记(2026-09-06,v1.7.0 基线)

| 项 | 值 |
|---|---|
| CPU | AMD Ryzen 9 7945HX(16C/32T,~2.5GHz 基频) |
| 编译器 | MinGW-w64 gcc **16.1.0**(x86_64-win32-seh) |
| flags | `-O2 -std=c99 -Wall -Wextra` |
| 系统 | Windows 11 / Git Bash;`clock()` 为进程 CPU 时间,粒度 ~1ms |
| 查表变体 | 追加 `-DET_CRC_TABLE=1` 单独构建(CRC16-CCITT 与 CRC32 v1.9 起同受益) |

## 结果(v1.7.0 基线)

| 基准 | 位算法构建 | 查表构建 |
|---|---|---|
| ringbuf 单字节(写1读1,cap 4096) | 173.4 MB/s | — |
| ringbuf 块 256B(cap 4096 POW2) | 1220.7 MB/s | — |
| ringbuf 块 256B(cap 4000 非 POW2) | 1220.7 MB/s | — |
| crc16-ccitt(4KB×512,链式) | 500.0 MB/s | 500.0 MB/s |
| crc16-modbus(4KB×512,链式;v2.2) | 142.9 MB/s(位) | 500.0 MB/s(表) |
| crc32(4KB×512,链式; v1.9 前恒位算法) | 125.0 MB/s(位) | 500.0 MB/s(表) |
| xmodem 有效载荷吞吐(128B 块,含协议开销) | 271.3 MB/s | — |
| kv set+get 交替(32B 值,host 虚拟 flash) | 90909 ops/s | — |
| filter movavg 单次更新 | 2.0 ns/op | — |
| fsm dispatch(含 guard 回调) | 4.0 ns/op | — |
| map u32 get(97 槽,负载 0.62;v1.9) | 2.0 ns/op | — |
| smap str get(97 槽,负载 0.62;v1.9) | 8.0 ns/op | — |
| smap ci get(大小写折叠查,v2.0) | 11.0 ns/op | — |
| pid step(P+I+D, d-on-measure;v2.1) | 15.0 ns/op | — |
| stats push(Welford 整数;v2.1) | 6.0 ns/op | — |
| medfilt push(win 5;v2.2) | 11.0 ns/op | — |
| sched poll_once(1 任务到期+耗时计量;v2.2) | 5.0 ns/op | — |
| hist push(16 桶;v2.3) | 2.0 ns/op | — |
| hist percentile(16 桶,插值;v2.3) | 10.0 ns/op | — |

## 观察与备注

- ringbuf POW2 与非 POW2 块吞吐无差异:该访问模式下内部操作相同(无取模,自动递增索引);
- crc16-ccitt 位算法与查表在本模式(4KB 驻 L1 缓冲)下同达吞吐上限,查表的价值预期在缓存敏感/更大缓冲场景;
- crc32 位算法 125 MB/s(≈ ccitt 位算法的 1/4,反射+32 位宽所致);v1.9 补表路径后 500 MB/s,与 ccitt 表同档;
- xmodem 有效吞吐含帧头/补码/CRC 计算(133B 线路字节承载 128B 载荷,协议开销 3.9%);
- kv 速率受 host 虚拟 flash(memcpy 模拟)影响,**不代表真实 flash 时序**(真实页擦 ms 级);
- `pid step` 含 P/I/D 三项 + 饱和 int64 乘除, 15 ns/op 量级(定点闭环 10kHz 采样下 < 0.02% CPU);
- `stats push` Welford 单样本更新 6 ns/op, 与 filter movavg(2 ns/op)同档, 适合在线长期统计;
- `medfilt push` 小窗插入排序 O(win²): win=5 为 11 ns/op, 量级与 smap 查找同档; win 增到 15 时按平方律增长, 高频采样场景窗口别贪大;
- `sched poll_once` 含时基读取×3 + 任务调用 + 耗时计量, 5 ns/op 量级 —— 任务耗时统计的诊断收益远大于计量开销;
- `hist push` O(1) 等宽映射 2 ns/op(与 movavg 同档); `percentile` 逐桶线性扫 10 ns/op(16 桶), 255 桶按桶数线性增长 —— 高频调用场景桶数别超配;
- crc16-modbus 位算法 142.9 MB/s → 查表 500.0 MB/s(v2.2 P5-3 补表后与 ccitt 同档, 反射位算法为 ccitt 的 ~1/3.5);
- 复现:`make bench`(或 Makefile 注释中的 gcc 命令),查表变体 `make bench-table`。

## 版本记录

| 版本 | 日期 | 变化 |
|---|---|---|
| v2.3.0 | 2026-09-12 | 复测无回归; 新增 hist push 2.0 / hist percentile 10.0 ns/op(直方图可观测, O(1) 入桶 + 桶内插值分位) |
| v2.2.0 | 2026-09-12 | 复测: pid/stats 在 `clock()` 1ms 粒度内波动(9~15/4~6 ns/op, 非回归); 新增 medfilt push 11.0、sched poll_once 5.0 ns/op; crc16-modbus 补表 142.9→500.0 MB/s |
| v2.1.0 | 2026-09-11 | 复测: 既有行在 `clock()` 1ms 粒度抖动内(如 crc16 400/500、smap 8/11 为 tick 级波动, 非回归); 新增 pid step 15.0 ns/op、stats push 6.0 ns/op |
| v2.0.0 | 2026-09-09 | 复测无回归; 新增 smap_ci 折叠查找行 11.0 ns/op(折叠循环 +3ns, 查表/命令路由可忽略) |
| v1.9.0 | 2026-09-08 | 复测无回归; 新增 map/smap 查找行(2/8 ns/op, 字符串键哈希+memcmp 开销 ~4x); crc32 表路径 125→500 MB/s |
| v1.8.0 | 2026-09-06 | 复测: 各项在噪声范围内(±5%), 无回归; map/xmodem_tx 未入基准(下版按需) |
| v1.7.0 | 2026-09-06 | 首版基线(本表全部数字) |
