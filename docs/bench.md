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
| 查表变体 | 追加 `-DET_CRC_TABLE=1` 单独构建(仅 CRC16-CCITT 受益) |

## 结果(v1.7.0 基线)

| 基准 | 位算法构建 | 查表构建 |
|---|---|---|
| ringbuf 单字节(写1读1,cap 4096) | 173.4 MB/s | — |
| ringbuf 块 256B(cap 4096 POW2) | 1220.7 MB/s | — |
| ringbuf 块 256B(cap 4000 非 POW2) | 1220.7 MB/s | — |
| crc16-ccitt(4KB×512,链式) | 500.0 MB/s | 500.0 MB/s |
| crc32 位算法(4KB×512,链式) | 125.0 MB/s | —(无表路径) |
| xmodem 有效载荷吞吐(128B 块,含协议开销) | 271.3 MB/s | — |
| kv set+get 交替(32B 值,host 虚拟 flash) | 90909 ops/s | — |
| filter movavg 单次更新 | 2.0 ns/op | — |
| fsm dispatch(含 guard 回调) | 4.0 ns/op | — |

## 观察与备注

- ringbuf POW2 与非 POW2 块吞吐无差异:该访问模式下内部操作相同(无取模,自动递增索引);
- crc16-ccitt 位算法与查表在本模式(4KB 驻 L1 缓冲)下同达吞吐上限,查表的价值预期在缓存敏感/更大缓冲场景;
- crc32 无查表路径,恒位算法(125 MB/s ≈ ccitt 的 1/4,反射+32 位宽所致);
- xmodem 有效吞吐含帧头/补码/CRC 计算(133B 线路字节承载 128B 载荷,协议开销 3.9%);
- kv 速率受 host 虚拟 flash(memcpy 模拟)影响,**不代表真实 flash 时序**(真实页擦 ms 级);
- 复现:`make bench`(或 Makefile 注释中的 gcc 命令),查表变体 `make bench-table`。

## 版本记录

| 版本 | 日期 | 变化 |
|---|---|---|
| v1.7.0 | 2026-09-06 | 首版基线(本表全部数字) |
