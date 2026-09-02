# Embedded_Tools API 指南

> 适用版本：v1.2 ｜ 语言标准：C99 ｜ 目标环境：裸机前后台循环（兼容任意 MCU）

---

## 目录

- [1. 全局约定](#1-全局约定)
- [2. core 数据结构层](#2-core-数据结构层)
  - [2.1 et_ringbuf 环形缓冲区](#21-et_ringbuf-环形缓冲区)
  - [2.2 et_queue 定长消息队列](#22-et_queue-定长消息队列)
  - [2.3 et_mempool 固定块内存池](#23-et_mempool-固定块内存池)
  - [2.4 et_list 侵入式双向链表](#24-et_list-侵入式双向链表)
- [3. algorithm 纯算法层](#3-algorithm-纯算法层)
  - [3.1 et_filter 定点滤波器组](#31-et_filter-定点滤波器组)
- [4. sys 系统服务层](#4-sys-系统服务层)
  - [4.1 et_stimer 软件定时器](#41-et_stimer-软件定时器)
  - [4.2 et_sched 任务调度器](#42-et_sched-任务调度器)
  - [4.3 et_event 事件标志组](#43-et_event-事件标志组)
  - [4.4 et_softclock 软时钟](#44-et_softclock-软时钟)
- [5. protocol 协议层](#5-protocol-协议层)
  - [5.1 et_crc 校验计算](#51-et_crc-校验计算)
  - [5.2 et_frame 帧解析器](#52-et_frame-帧解析器)
  - [5.3 et_atcmd 命令解析器](#53-et_atcmd-命令解析器)
- [6. storage 存储层](#6-storage-存储层)
  - [6.1 et_kv flash 键值存储](#61-et_kv-flash-键值存储)
- [7. drivers 设备驱动层](#7-drivers-设备驱动层)
  - [7.1 et_key 按键](#71-et_key-按键)
  - [7.2 et_led LED](#72-et_led-led)
  - [7.3 et_spwm 软件 PWM](#73-et_spwm-软件-pwm)
- [8. debug 调试层](#8-debug-调试层)
  - [8.1 et_log 日志](#81-et_log-日志)
  - [8.2 et_assert 断言](#82-et_assert-断言)
- [9. port 平台适配契约](#9-port-平台适配契约)
- [10. 配置项参考](#10-配置项参考)
- [11. 典型组合配方](#11-典型组合配方)

---

## 1. 全局约定

| 约定 | 说明 |
|---|---|
| **命名空间** | 所有公开符号以 `et_` 前缀 + 模块名开头；结构体成员以内部注释标明"勿动"的禁止外部修改 |
| **内存模型** | 零动态分配。所有实例由调用方创建（static / 栈 / 内存池），库内无 malloc |
| **多实例** | 一切操作经句柄 `et_xxx_t*`，同一类型可安全创建任意多实例 |
| **时基** | `uint32_t` 毫秒，单调递增、允许自然回绕。所有跨回绕比较已用无符号减法 / int32 差值处理 |
| **错误处理** | 初始化类函数返回 `bool`；数据操作类函数返回实际处理数量或状态布尔值。非法参数在 `ET_ASSERT` 开启时可被捕获 |
| **上下文标注** | 下文用 🔒ISR 表示可在中断中调用；🏠MAIN 表示仅限主循环上下文 |

头文件包含路径要求（编译时加入）：

```
-I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Iport
```

只需拷贝启用模块对应的 `.c/.h` 对即可使用，模块间无隐藏耦合。

---

## 2. core 数据结构层

### 2.1 et_ringbuf 环形缓冲区

SPSC 无锁设计：写者只更新 `head`，读者只更新 `tail`，索引自由递增，回绕由无符号减法自然消化，不牺牲存储槽位。

```c
#include "et_ringbuf.h"

typedef struct {
    volatile uint32_t head;      /* 内部状态, 勿动 */
    volatile uint32_t tail;
    uint32_t          size;
    uint8_t          *buf;
} et_ringbuf_t;
```

#### API

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_ringbuf_init(rb, storage, size)` | 任一 | 绑定外部存储区，size ≥ 1 |
| `void et_ringbuf_reset(rb)` | 🏠MAIN | 清空数据（head=tail=0），需无并发访问 |
| `uint32_t et_ringbuf_used(rb)` | 只读侧 | 已存字节数 |
| `uint32_t et_ringbuf_free_space(rb)` | 写侧 | 剩余可写字节数 |
| `bool et_ringbuf_is_empty(rb)` / `is_full(rb)` | 各侧 | 状态查询 |
| `uint32_t et_ringbuf_write(rb, data, len)` | 🔒写者 | 拷贝写入，返回实际写入数（满时部分写入） |
| `uint32_t et_ringbuf_read(rb, data, len)` | 🔒读者 | 拷贝读出并消费，返回实际读出数 |
| `uint32_t et_ringbuf_peek(rb, out, len)` | 🔒读者 | 窥视但不消费 |
| `void et_ringbuf_drop(rb, len)` | 🔒读者 | 丢弃 len 字节（配合 peek 使用） |
| `uint8_t *et_ringbuf_write_reserve(rb, want, got)` | 🔒写者 | 取连续可写区（见下） |
| `void et_ringbuf_write_commit(rb, len)` | 🔒写者 | 发布 reserve 区域内的 len 字节 |
| `const uint8_t *et_ringbuf_read_peek(rb, want, got)` | 🔒读者 | 取连续可读区（到物理末尾截断） |

#### 并发规则

- ✅ 一个写者（如 UART ISR）+ 一个读者（主循环）：**无需任何加锁**
- ❌ 多写者或多读者：调用方自行用 `PORT_CRITICAL_*` 包裹
- ❌ 同一侧操作不可重入（如两个 ISR 同时 write）

#### 零拷贝用法

```c
/* DMA 式收包: 先取连续区 -> 填充 -> 提交 */
uint32_t got;
uint8_t *p = et_ringbuf_write_reserve(&rb, dma_len, &got);
if (p != NULL) {
    memcpy(p, hw_fifo, got);            /* 只填 got 字节 */
    et_ringbuf_write_commit(&rb, got);
}

/* 零拷贝消费 */
uint32_t n;
const uint8_t *r = et_ringbuf_read_peek(&rb, 64, &n);
process(r, n);
et_ringbuf_drop(&rb, n);
```

配置：`ET_RINGBUF_POW2=1` 时容量必须为 2 的幂，取模优化为位与。

---

### 2.2 et_queue 定长消息队列

元素等长的 FIFO，采用与 ringbuf 相同的自由递增索引技巧。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_queue_init(q, storage, storage_size, item_size)` | 任一 | 容量 = storage_size / item_size（向下取整，须 ≥1） |
| `void et_queue_reset(q)` | 🏠MAIN | 清空 |
| `uint32_t et_queue_count(q)` / `capacity(q)` | 读 | 元素计数 |
| `bool et_queue_is_empty(q)` / `is_full(q)` | 读 | 状态 |
| `bool et_queue_push(q, item)` | 🔒生产者 | 队满返回 false，不覆盖旧数据 |
| `bool et_queue_pop(q, item)` | 🔒消费者 | 队空返回 false 且不改写 item |

```c
typedef struct { uint8_t cmd; int16_t value; } evt_t;
static et_queue_t q;
static uint8_t    q_mem[10 * sizeof(evt_t)];

et_queue_init(&q, q_mem, sizeof(q_mem), sizeof(evt_t));

/* ISR 中 */
evt_t e = { .cmd = 1, .value = 100 };
et_queue_push(&q, &e);

/* 主循环中 */
evt_t out;
while (et_queue_pop(&q, &out)) { handle(&out); }
```

并发规则与 ringbuf 相同（SPSC 无锁）。

---

### 2.3 et_mempool 固定块内存池

位图管理的定长块分配器。存储区自动布局为 `[位图][对齐填充][块区]`。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `size_t et_mempool_bytes_needed(block_size, block_count)` | 任一 | 计算所需存储字节数（含位图与对齐开销） |
| `bool et_mempool_init(mp, storage, storage_size, block_size, block_count)` | 任一 | 空间不足返回 false |
| `void *et_mempool_alloc(mp)` | 🏠MAIN | 成功返回块指针；耗尽返回 NULL。带扫描提示，常规近似 O(1) |
| `bool et_mempool_free(mp, ptr)` | 🏠MAIN | STRICT 模式下拒绝越界指针与重复释放 |
| `uint32_t et_mempool_free_count(mp)` | 读 | 空闲块数 |
| `bool et_mempool_contains(mp, ptr)` | 读 | ptr 是否为本池内按块对齐的地址（不含分配状态判断） |

⚠️ **非中断安全**：ISR 与主循环共享时须整体用临界区包裹 alloc/free。

```c
#define N 8
static et_mempool_t pool;
static uint8_t      pool_mem[et_mempool_bytes_needed(64, N)];  /* 编译期算大小 */

et_mempool_init(&pool, pool_mem, sizeof(pool_mem), 64, N);

uint8_t *buf = et_mempool_alloc(&pool);
if (buf != NULL) { use(buf); et_mempool_free(&pool, buf); }
```

配置：`ET_MEMPOOL_ALIGN`（默认指针宽度）、`ET_MEMPOOL_STRICT`（默认开，发布可关提速）。

---

### 2.4 et_list 侵入式双向链表

零拷贝组织任意结构：节点 `et_list_node_t` 嵌入用户结构体，链表不拷贝不分配数据；任意位置 O(1) 插入/删除。与 `et_queue`（值拷贝 FIFO）互补。

```c
typedef struct et_list_node {           /* 嵌入用户结构体, 成员勿动 */
    struct et_list_node *prev;
    struct et_list_node *next;
} et_list_node_t;

typedef struct {
    et_list_node_t head;                /* 哨兵: head.next=首, head.prev=尾 */
    uint32_t       count;               /* 当前节点数 */
} et_list_t;
```

#### API

| 函数 | 上下文 | 说明 |
|---|---|---|
| `void et_list_init(l)` | 任一 | 得到空表 |
| `void et_list_node_init(n)` | 任一 | 节点置"不在链上"（静态结构体置零等效） |
| `bool et_list_push_back(l, n)` / `push_front` | 🏠MAIN | 节点已在任一链表上时返回 false |
| `bool et_list_remove(l, n)` | 🏠MAIN | O(1)；未链接/重复移除返回 false 且不动链表 |
| `et_list_node_t *et_list_front(l)` / `back(l)` | 读 | 空表返回 NULL |
| `bool et_list_is_empty(l)` / `uint32_t et_list_count(l)` | 读 | 状态 |
| `void et_list_foreach(l, fn, user)` | 🏠MAIN | 遍历；回调中删除当前节点/其后继/任意未访问节点均安全 |

宏 `ET_LIST_CONTAINER(node_ptr, type, member)`：由节点指针还原宿主结构体指针。

#### 并发规则

- ✅ 单上下文模块：默认主循环使用，所有 API 同一上下文内无需加锁
- ❌ 跨上下文共享（ISR 插入/移除）：调用方用 `PORT_CRITICAL_*` 包裹**完整操作序列**（含遍历全程）

```c
typedef struct { et_list_node_t node; int id; } item_t;   /* 节点嵌入用户结构 */
static et_list_t list;
static item_t    pool[8];

et_list_init(&list);
et_list_push_back(&list, &pool[0].node);
et_list_push_back(&list, &pool[1].node);

/* 遍历中自删安全 */
static void visit(et_list_node_t *n, void *user) {
    item_t *it = ET_LIST_CONTAINER(n, item_t, node);
    if (it->id == 3) et_list_remove(&list, n);
}
et_list_foreach(&list, visit, NULL);
```

实现约定（白盒知识）：`node->prev != NULL` 即"在链上"；`et_list_remove` 后 `node->next` 保留指向原后继（供 foreach 安全遍历），其余状态视为无效。节点归属校验不做（O(1) 代价约束），跨链表误删属调用方违例。

---

## 3. algorithm 纯算法层

信号处理纯算法，**禁止包含 port.h**（与 core 同级纪律），全部定点、零浮点依赖、零动态内存。

### 3.1 et_filter 定点滤波器组

三个独立滤波器，句柄互不兼容，可级联组合。

#### 滑动窗口均值 `et_movavg_t`

环形覆盖历史样本，O(1) 增量更新（减旧加新）；和值 int64 承载不溢出。窗口未满时输出已有样本均值（开机免长等待）。

| 函数 | 说明 |
|---|---|
| `bool et_movavg_init(f, storage, window)` | 绑定样本存储区（容量 ≥ window 个 int32），window ≥ 1 |
| `int32_t et_movavg_update(f, x)` | 送入样本，返回均值（对称四舍五入） |
| `void et_movavg_reset(f)` | 清空历史 |
| `et_movavg_count(f)` / `window(f)` | 当前样本数 / 窗口容量 |

#### 一阶 IIR 低通 `et_lpf1_t`

递推式 `y += k*(x-y)`，k 为 Q15 定点系数（0~32767）。首样本直通。**定点特性**：稳态存在 `|残差| < 32768/k` 的死区（k=32767 时约 1 LSB）。

| 函数 | 说明 |
|---|---|
| `bool et_lpf1_init(f, k_q15)` | k_q15=0 输出冻结；32767 近似直通 |
| `int32_t et_lpf1_update(f, x)` | 送入样本，返回滤波输出 |
| `void et_lpf1_set_k(f, k_q15)` | 运行中调系数 |
| `void et_lpf1_reset(f)` / `et_lpf1_output(f)` | 重新直通 / 读当前输出 |

#### 斜率限制 `et_slew_t`

每步最多向目标移动 `max_step`，抑制脉冲毛刺；限幅内无损直通。首样本直通。

| 函数 | 说明 |
|---|---|
| `bool et_slew_init(f, max_step)` | max_step ≥ 1 |
| `int32_t et_slew_update(f, x)` | 送入样本，返回限幅输出 |
| `void et_slew_reset(f)` / `et_slew_output(f)` | 重新直通 / 读当前输出 |

```c
static et_movavg_t ma; static int32_t ma_buf[8];
static et_lpf1_t   lp;

et_movavg_init(&ma, ma_buf, 8u);
et_lpf1_init(&lp, 8192u);               /* α = 0.25 */

int32_t raw = adc_read();
raw = et_movavg_update(&ma, raw);       /* 先抑工频噪声 */
int32_t smooth = et_lpf1_update(&lp, raw);   /* 再平滑 */
```

上下文约定：单上下文模块（典型为采样任务），跨上下文由调用方加临界区。

---

## 4. sys 系统服务层

三个模块共享同一个毫秒时基（来自 `port_tick_get_ms()`），互不冲突，可按需选用。

### 4.1 et_stimer 软件定时器

轮询式软定时器，不占用硬件定时器资源。回调在临界区外执行，回调内可安全启停任何定时器（含自身）。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_stimer_init(t, cb, arg)` | 任一 | cb 必须非空；运行中的定时器禁止重复 init |
| `bool et_stimer_start_oneshot(t, delay_ms)` | 🔒ISR | delay ≥ 1；运行中重启则重新计时 |
| `bool et_stimer_start_periodic(t, period_ms)` | 🔒ISR | period ≥ 1 |
| `bool et_stimer_stop(t)` | 🔒ISR | 未在运行返回 false |
| `bool et_stimer_is_running(t)` | 读 | 状态 |
| `void et_stimer_poll(now)` | 🏠MAIN | 分发全部到期回调，now 通常取 `port_tick_get_ms()` |
| `void et_stimer_reset_all(void)` | 🏠MAIN | 解除全部注册并停止（热复位场景） |

**周期追赶语义**：错过多个周期时逐次补发（平均频率不变）。若不希望停顿后出现补发脉冲，请改用单次模式自行重挂载。

**时长上限**：单次延迟 / 周期须 `< 2^31 ms`（内部 int32 差值判到期，时基回绕依然正确）。

```c
static et_stimer_t hb;

static void heartbeat(void *arg) { led_toggle(); }

et_stimer_init(&hb, heartbeat, NULL);
et_stimer_start_periodic(&hb, 500);

while (1) {
    et_stimer_poll(port_tick_get_ms());
    /* ... 其他任务 ... */
}
```

### 4.2 et_sched 任务调度器

协作式周期任务调度，FIFO 尾插保证公平；任务错过的周期只补跑一次（重锚定 now），避免积压风暴。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_sched_register(t, fn, arg, period_ms)` | 🏠MAIN | period ≥ 1；重复注册同一任务拒绝 |
| `bool et_sched_unregister(t)` | 🏠MAIN | 未注册返回 false |
| `void et_sched_poll_once(void)` | 🏠MAIN | 扫描并执行一遍到期任务后返回（非阻塞，可配 WFI） |
| `void et_sched_reset(void)` | 🏠MAIN | 注销全部 |

⚠️ 与 stimer 不同，本模块**全部 API 仅限主循环**——因此内部零临界区。ISR 与调度任务的交互请走 `et_event` 或 `et_queue`。

```c
static et_task_t t_key, t_log;

et_sched_register(&t_key, key_scan_task, NULL, 10);   /* 10ms 扫描 */
et_sched_register(&t_log, report_task,   NULL, 1000);

while (1) {
    et_sched_poll_once();
    enter_low_power_until_next_tick();                /* 低功耗可选 */
}
```

**stimer vs sched 选择**：需要动态启停 / 单次延迟 / ISR 控制 → stimer；固定周期后台活 → sched。

### 4.3 et_event 事件标志组

32 个独立事件位的轻量同步原语，全部 API 任意上下文可用（内部临界区保护读改写）。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `void et_event_init(g)` | 任一 | 清零 |
| `void et_event_set(g, bits)` | 🔒ISR | 按"或"累积，多位可一次置位 |
| `uint32_t et_event_peek(g)` | 读 | 当前未清除的事件位 |
| `uint32_t et_event_wait_and_clear(g, mask)` | 建议🏠 | 取走 mask 命中的位并清除，返回实际取到的位 |
| `void et_event_clear(g, bits)` | 任一 | 强制清除 |

```c
#define EV_RX      (1u << 0)
#define EV_TIMEOUT (1u << 3)

/* UART ISR */
et_event_set(&evt, EV_RX);

/* 主循环 */
uint32_t got = et_event_wait_and_clear(&evt, EV_RX | EV_TIMEOUT);
if (got & EV_RX)  process_rx();
if (got & EV_TIMEOUT) do_timeout();
```

### 4.4 et_softclock 软时钟 (timestamp → 日历)

纯软件换算：把调用方注入的毫秒 tick（如 `port_tick_get_ms()`）推进为 **UTC 日历时间**。Hinnant `civil_from_days` 纯整数算法（无循环无查表），闰年/月末自动处理；内部为 32 位无符号秒计数，覆盖 **1970 ~ 2106 年**（无 2038 问题）。断电恢复：把 `et_softclock_unix()` 值经 et_kv 持久化，上电 `et_softclock_init(sc, 保存值)` 恢复（demo 见 `examples/stm32f103_demo.c`）。

```c
#include "et_softclock.h"

et_softclock_t sc;
et_datetime_t  dt;

et_softclock_init(&sc, 1767225600u);        /* 2026-01-01 00:00:00 UTC */
while (1) {
    et_softclock_poll(&sc, port_tick_get_ms());   /* 主循环每趟推进 */
    et_softclock_get_datetime(&sc, &dt);          /* → {2026,1,1, 0,0,12} */
}
```

| API | 说明 |
|---|---|
| `et_softclock_init(sc, unix_sec)` | 设定起始时间（可由 et_kv 恢复） |
| `et_softclock_set_unix(sc, unix_sec)` | 运行中校时（NTP/恢复） |
| `et_softclock_poll(sc, now_ms)` | 推进；无符号减法消化回绕与大步进 |
| `et_softclock_unix(sc)` | 当前 UTC 秒（30 位, 1970-2106） |
| `et_softclock_get_datetime(sc, &dt)` | 拆解为 年/月/日/时/分/秒 |

已覆盖：闰年/闰世纪、月末进位、回绕、多实例、毫秒余数累计等 13 例单测（`test/test_softclock.c`）。

---

## 5. protocol 协议层

### 5.1 et_crc 校验计算

位算法实现，零查表内存；流式 API 供帧解析等场景增量计算。

| 族系 | 参数 | 一次性 | 流式种子 |
|---|---|---|---|
| CRC8 | poly 0x07 | `et_crc8(data,len)` | `ET_CRC8_INIT` (0x00) |
| CRC16/MODBUS | 反射 0xA001 | `et_crc16_modbus(data,len)` | `ET_CRC16_MODBUS_INIT` (0xFFFF) |
| CRC16/CCITT-FALSE | poly 0x1021 | `et_crc16_ccitt(data,len)` | `ET_CRC16_CCITT_INIT` (0xFFFF) |
| CRC32/IEEE | zlib/PNG 兼容 | `et_crc32(data,len)` | `ET_CRC32_INIT` (0xFFFFFFFF) |

流式函数签名统一为 `type update(type crc, const void *data, uint32_t len)`：

```c
uint16_t c = ET_CRC16_MODBUS_INIT;
c = et_crc16_modbus_update(c, hdr,  hdr_len);
c = et_crc16_modbus_update(c, body, body_len);
/* c 即整帧校验 */
```

标准向量自检：对 `"123456789"` 分别得 0xF4 / 0x4B37 / 0x29B1 / 0xCBF43926。

### 5.2 et_frame 帧解析器

可裁剪的字节流帧格式：

```
[帧头 1~4B][长度域 1B?][载荷 nB][校验 0~2B?][帧尾 1B?]
```

- 长度域 = 载荷字节数（0~255）；关闭长度域则使用固定载荷长度
- 校验覆盖：长度域（若有）+ 载荷，不含帧头/帧尾
- CRC16-MODBUS 校验字节低前高后；CCITT 高前低后
- 载荷直接写入调用方缓冲，解析器零额外 RAM

#### 配置结构 `et_frame_cfg_t`

| 字段 | 说明 |
|---|---|
| `header, header_len` | 帧头字节序列，长度 1~4 |
| `use_len` | true：帧内含 1 字节长度域 |
| `fixed_len` | use_len=false 时的定长载荷 |
| `tail, use_tail` | 可选帧尾字节 |
| `crc` | `ET_FRAME_CRC_NONE / XOR / SUM8 / CRC8 / CRC16_MODBUS / CCITT` |
| `rx_buf, rx_cap` | 调用方提供的载荷接收缓冲 |
| `on_frame(p, len, user)` | 完帧回调，可为 NULL；在 feed 上下文中同步执行（可能在 ISR！） |
| `user` | 回调用户指针 |

#### API

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_frame_parser_init(p, cfg)` | 任一 | 拷贝配置（cfg 之后可释放）；帧头长度非法返回 false |
| `void et_frame_reset(p)` | 🏠MAIN | 回到帧头扫描态（不清统计） |
| `bool et_frame_feed(p, byte)` | 🔒单写者 | 喂 1 字节；完成一帧有效帧时返回 true |
| `uint32_t et_frame_write(p, data, len)` | 🔒单写者 | 批量喂入，返回完成的帧数 |
| `uint16_t et_frame_pack(cfg, payload, len, out, out_cap)` | 任一 | 组帧，返回总字节数（容量不足/参数非法返回 0） |

容错行为：帧头部分匹配自动回退（AA AA 55 正确同步）；坏校验/超长/帧尾错 → `err_count++` 并复位重新同步。统计字段 `frame_count / err_count` 可随时读取。

```c
static const uint8_t HDR[2] = {0xAA, 0x55};
static et_frame_parser_t parser;
static uint8_t pay[64];

void uart_init_frames(void) {
    et_frame_cfg_t cfg = {
        .header = HDR, .header_len = 2,
        .use_len = true, .crc = ET_FRAME_CRC_CRC16_MODBUS,
        .rx_buf = pay, .rx_cap = sizeof(pay),
        .on_frame = my_handler,
    };
    et_frame_parser_init(&parser, &cfg);
}

void USART_IRQHandler(void) {          /* ISR 里逐字节喂 */
    et_frame_feed(&parser, usart_read_byte());
}
```

### 5.3 et_atcmd 命令解析器

行式命令协议：`AT+<名称>[空格<参数...>]\r\n`。CR/LF 自适应、支持退格（0x08/0x7F）、行超长自动丢弃恢复。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_atcmd_init(p, cmds, cmd_count, linebuf, line_cap, user)` | 任一 | line_cap ≥ 8；命令表由调用方持有 |
| `void et_atcmd_reset(p)` | 🏠MAIN | 丢弃半行输入 |
| `bool et_atcmd_feed(p, ch)` | 🔒单写者 | 仅当本字节完成一条有效命令时返回 true |
| `char *et_atcmd_next_arg(cursor)` | - | 参数切分辅助：按单个空格原地切分，耗尽返回 NULL |

命令名大小写敏感；参数为去除前导空格后的行内原文（允许含空格，用 next_arg 切分）。未匹配命令触发 `on_unknown`（可为 NULL）。

```c
static void cmd_ver(char *args, void *user) {
    (void)args; (void)user;
    reply("V1.0");
}
static const et_atcmd_entry_t cmds[] = {
    { "VER", cmd_ver },
    { "RST", cmd_rst },
};
static et_atcmd_proc_t at;
static char line[48];

et_atcmd_init(&at, cmds, 2u, line, sizeof(line), NULL);

/* 完帧回调里接力(见 11.3 组合配方) */
char ch;
while (get_char(&ch)) et_atcmd_feed(&at, ch);
```

---

## 6. storage 存储层

### 6.1 et_kv flash 键值存储 (掉电保存)

"参数掉电保存"专用 KV 库：双扇区乒乓 + 追加写 + CRC32，断电安全，零动态内存，仅依赖 `port.h` 的 flash 三件套（`ET_MODULE_KV=1` 时 port 必须实现 flash 契约，见 9 章）。

```c
#include "et_kv.h"

et_kv_t        kv;
et_kv_layout_t lay = { 14u, 15u };     /* 参数区内两个扇区, 平台自定 */
uint32_t       boot = 0u;

if (!et_kv_init(&kv, &lay)) {          /* 两页均坏(首上电/误擦除) */
    et_kv_format(&kv, &lay);           /* 丢弃旧数据重建 */
    et_kv_init(&kv, &lay);
}
et_kv_get(&kv, 1, &boot, sizeof(boot), NULL);
boot++;
et_kv_set(&kv, 1, &boot, sizeof(boot));   /* 即时持久化 */
```

#### 可靠性模型

| 环节 | 机制 |
|---|---|
| 页头 | `magic('ETKV') \| seq \| state \| crc32`（CRC 仅覆盖前 8 字节，state 字段独立于校验） |
| 有效判定 | state 逐位编程 0xFFFFFFFF(MOVING)→0x00000000(COMMITTED)，是页生效前的**最后一次写**：搬迁中掉电 → 废弃半成品页、源页无损 |
| 记录 | key u16（bit15=Tombstone）\| len \| vcrc(=crc32(payload)) \| payload 4B对齐+0xFF 尾；单个损坏只丢该记录 |
| 断电容忍 | 页头/记录任意位置掉电，重开自动迁移修复；读出 CRC 失败仅跳过，不影响其他 key |
| 满页 | 自动压实（专用 → 备用 → 换代 seq+1）；`et_kv_commit()` 可手动抳 |

#### API

| 函数 | 说明 |
|---|---|
| `et_kv_init(kv, layout)` | 扫描两扇区仲裁选活跃页；脏尾自动搬迁修复；两页均坏返回 false |
| `et_kv_format(kv, layout)` | 擦除两扇区 + 写 seq=1 空白页（旧数据全弃） |
| `et_kv_set(kv, key, val, len)` | 追加新版本（即时持久化）；空间不足自动压实重试；len≤单页上限 |
| `et_kv_get(kv, key, buf, cap, *out_len)` | 取最新有效版本；不存在/已删/CRC 坏返回 false（不触碰 buf） |
| `et_kv_del(kv, key)` | 写 tombstone；key 不存在时零写入返回 false |
| `et_kv_size(kv, key)` | 查询当前值长度（不存在/已删/损坏为 0） |
| `et_kv_commit(kv)` | 手动压实：存活记录（去重）迁去对页 |
| `et_kv_stats(kv, &st)` | seq / 双扇区擦除计数 / 已用剩余字节 / 槽位与有效 key 数 |

用户 key ∈ [1, 0x7FFE]（0x7FFF、0xFFFF 为系统哨兵）。全部 API 仅限 🏠MAIN 上下文（flash 擦写为 ms 级阻塞，喂狗由调用方负责）。

#### 断电恢复测试矩阵（test/test_kv.c）

覆盖：首启格式化、追加/覆盖/删除、页头截断、记录截断、压缩前/中/后各断点、半扇区残留、seq 仲裁、写违例注入 —— 28 例全绿；掉电点靠宿主机 flash 模拟器“一次性截断 + 位写约束”注入（见 port/host/）。

---

## 7. drivers 设备驱动层

三个驱动均通过回调抽象硬件（电平读取 / 亮度写出 / 电平写出），不直接依赖 port GPIO——矩阵键盘、IO 扩展器、软件 PWM 均可接入。

### 7.1 et_key 按键

四态 FSM：双向时间戳消抖。事件序约定：短按结束时先 `RELEASE` 后 `CLICK`；长按后释放只有 `RELEASE` 不产生 `CLICK`。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_key_init(k, read, on_event, user, prm)` | 任一 | prm=NULL 用默认参数(20/600/0)；read 返回"已按下"（高有效归一化后） |
| `void et_key_scan(k, now)` | 🏠MAIN | 每 5~20ms 调一次；now 为当前毫秒 |

参数 `et_key_params_t`：

| 字段 | 含义 | 典型值 |
|---|---|---|
| `debounce_ms` | 双向消抖时间 | 15~30 |
| `long_press_ms` | 长按阈值 | 400~800 |
| `repeat_ms` | 连发间隔，0=关闭 | 100~150 |

事件枚举：`ET_KEY_PRESS / RELEASE / CLICK / LONG_PRESS / REPEAT`。

```c
static bool key_read(void *u) { return (GPIOA->IDR & PIN) == 0; }  /* 低有效转高有效 */

static void on_key(struct et_key *k, et_key_event_t ev, void *u) {
    if (ev == ET_KEY_CLICK) toggle_relay();
    else if (ev == ET_KEY_LONG_PRESS) factory_reset();
}

static et_key_t key;
et_key_params_t p = { 20, 600, 0 };
et_key_init(&key, key_read, on_key, NULL, &p);

/* 主循环 10ms 一次 */
et_key_scan(&key, port_tick_get_ms());
```

组合键建议在应用层实现：维护各键 `is_pressed` 位图 + 超时窗口判定，不污染单键 FSM。

### 7.2 et_led LED

模式驱动，相位基于绝对时基计算（轮询抖动不影响闪烁精度）；输出带缓存，亮度不变时不触碰硬件。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_led_init(l, write, user)` | 任一 | 默认熄灭 |
| `void et_led_set_off(l)` / `set_on(l)` | 任一 | 常灭/常亮 |
| `bool et_led_set_blink(l, period_ms, duty_pct, times)` | 任一 | period≥10；duty 1~99；times=0 表示无限，否则闪 N 个周期后自熄 |
| `bool et_led_set_breath(l, period_ms)` | 任一 | 三角波呼吸，period≥100 |
| `void et_led_poll(l, now)` | 🏠MAIN | 重算亮度输出；BLINK 类低频轮询即可，BREATH 建议周期/64 以上频率 |

`write(user, brightness)` 的 brightness 为 0~255：普通 GPIO 可阈值化（>127 视为亮），PWM 平台直通占空比。

```c
static void led_out(void *u, uint8_t v) { pwm_set_duty(CH, v); }

static et_led_t led;
et_led_init(&led, led_out, NULL);
et_led_set_blink(&led, 400, 50, 5);     /* 400ms 周期闪 5 次 */

/* 主循环里 */
et_led_poll(&led, port_tick_get_ms());
```

### 7.3 et_spwm 软件 PWM

多通道软件 PWM：相位基于绝对时基计算（`((now-t0) % period) < on_ms`），轮询抖动不累积相位误差；时基自然回绕由无符号减法消化。输出带缓存，电平不变时不重复调用 write。duty 0/255 为精确边界（恒低/恒高，任何相位无毛刺）。

**分辨率假设（务必阅读）**：1ms 时基下可用周期 ≥ 2ms（最高约 500Hz），占空比步进 1/period；主循环长阻塞会造成边沿抖动——高分辨率/高频需求请用硬件 PWM。

通道为静态注册表（容量 `ET_SPWM_CH_MAX`，默认 4），全部 API 仅限 🏠MAIN。

| 函数 | 上下文 | 说明 |
|---|---|---|
| `bool et_spwm_init(ch, fn, user, period_ms)` | 🏠MAIN | period_ms ≥ 2；默认 duty 0 恒低；重复 init 等效重配 |
| `void et_spwm_deinit(ch)` | 🏠MAIN | 停止驱动（不触碰最后一次输出电平） |
| `bool et_spwm_set(ch, duty_0_255)` | 🏠MAIN | 生效于下一次 poll；未初始化通道返回 false |
| `et_spwm_get_duty(ch)` / `get_period(ch)` | 读 | 未初始化返回 0 |
| `void et_spwm_poll(now)` | 🏠MAIN | 刷新全部通道，挂进主循环 poll 链 |

```c
static void ch0_out(void *u, uint8_t on) {       /* 电平回调: on 非 0 为高 */
    (void)u;
    gpio_write(LED_PIN, on);
}

et_spwm_init(0, ch0_out, NULL, 2);               /* 500Hz, 步进 1/2ms */
et_spwm_set(0, 128);                             /* ~50% */

while (1) {
    uint32_t now = port_tick_get_ms();
    et_spwm_poll(now);                           /* 与其他 poll 并列挂链 */
    ...
}
```

与 `et_led` 配套：把 et_led 的 write 回调直连 `et_spwm_set`，即可在无硬件 PWM 的 GPIO 上实现呼吸灯（见 11.5）。

---

## 8. debug 调试层

### 8.1 et_log 日志

输出格式：`[时基ms][级别字符][标签] 正文\n`，级别字符 T/D/I/W/E。
底层经 `port_putc()` 输出；默认不加锁，多上下文并发可能交错（需要原子行请上层加临界区）。

**两级开关：**

1. 编译期裁剪：`ET_LOG_MAX_LEVEL`（数值越小越详细）以上的宏整体消失，零体积；
2. 运行期过滤：`et_log_set_level()` 动态调整，低于过滤线的直接丢弃。

| API | 说明 |
|---|---|
| `void et_log_set_level(et_log_level_t lv)` | 运行时过滤线，超界自动收敛 |
| `et_log_level_t et_log_get_level(void)` | 当前过滤线 |
| `int et_log_output(lv, tag, fmt, ...)` | 核心输出，返回字符数（被过滤返回 -1） |
| `int et_log_raw(fmt, ...)` | 无前缀裸输出 |
| `void et_log_hexdump(lv, tag, data, len)` | 十六进制转储（偏移 4 位十六进制 + 16 字节/行 + ASCII 列） |

便捷宏：`ET_LOGT / ET_LOGD / ET_LOGI / ET_LOGW / ET_LOGE(tag, fmt, ...)`。

格式化支持：`%d %i %u %x %X %c %s %p %%` 及 `l/ll` 修饰（如 `%llu`）。不支持浮点与域宽。

```c
et_log_set_level(ET_LOG_LEVEL_INFO);       /* 发布可改 ERROR */

ET_LOGI("uart", "rx %u bytes", n);         /* [12345][I][uart] rx 12 bytes */
ET_LOGE("net", "crc mismatch");
et_log_hexdump(ET_LOG_LEVEL_DEBUG, "rx", buf, len);
```

编译期一刀切示例：`-DET_LOG_MAX_LEVEL=4`（仅保留 ERROR 及以上）。

### 8.2 et_assert 断言

| API | 说明 |
|---|---|
| `void et_assert_install(fail_fn, user)` | 注册失败钩子；传 NULL 恢复内置策略 |
| `void et_assert_fail(file, line, expr)` | 失败入口（通常经宏调用） |
| `ET_DBG_ASSERT(cond)` | 仅定义了 `ET_DEBUG` 时生效，否则零开销 |

失败处理策略：
- 已装钩子 → 调用钩子后**继续执行**（是否复位由钩子决定）；
- 无钩子（内置）→ 经 et_log 输出 `file:line expr(...)` 后原地停机（等待调试器/看门狗）。

```c
static void on_assert_fail(const char *f, int l, const char *e, void *u) {
    (void)u;
    save_coredump(f, l, e);        /* 记录现场 */
    system_reset();                /* 钩子内不复位则执行流继续 */
}
et_assert_install(on_assert_fail, NULL);

ET_DBG_ASSERT(queue != NULL);      /* 仅调试构建存在 */
```

---

## 9. port 平台适配契约

移植整个库需要实现 `port/port.h` 声明的能力（参考实现见 `port/host/`、`port/stm32f103/`）：

| 接口 | 要求 | 典型实现 |
|---|---|---|
| `PORT_CRITICAL_ENTER() / EXIT()` | 可嵌套临界区，保护主循环与 ISR 共享数据 | Cortex-M: 保存/恢复 PRIMASK + CPSID/CPSIE |
| `port_critical_enter()/exit()` | 同上（宏的函数形态） | 同上 |
| `port_tick_get_ms()` | 毫秒单调时基，由中断维护，允许自然回绕 | SysTick 中断累加 |
| `port_putc(c)` | 阻塞式单字符输出（日志底层） | USART 轮询发送 |
| `port_flash_read/write/erase_sector` + 几何宏 | **仅 ET_MODULE_KV=1 时必选**：参数区 4B 对齐只擦写、只允许 1→0 写、短写如实上报（掉电/故障部分写入） | F103: FLASH 寄存器直驱（PM0056） |

flash 契约要点（详见 `port/port.h` 与 `docs/proposals/et_kv_flash_contract.md`）：
- 参数区 = `PORT_FLASH_SECTOR_SIZE × PORT_FLASH_SECTOR_COUNT` 连续扇区，`PORT_FLASH_ERASE_MS_MAX` 给调用方喂狗参考；
- 擦/写必须发生在 🏠MAIN（内建临界区），ms 级阻塞由调用方安排调度与看门狗；
- `port_flash_write` 返回实际写入字节数（掉电可短写）——et_kv 靠它判别中断并恢复。

分层纪律：**core/algorithm 层禁止包含 port.h**（保证纯逻辑可 PC 测试）；sys/drivers/debug/storage 仅经此契约触硬件。

### 9.1 已验证平台

| 平台 | 编译 | 仿真 | 真机实测 | 记录 |
|---|---|---|---|---|
| host（gcc / clang，CI ubuntu+windows） | ✅ | ✅（虚拟 flash+时基） | ✅ 191 用例 | v1.0 起 |
| STM32F103C8T6（arm-none-eabi-gcc 13.3，`port/stm32f103/`） | ✅ 零警告 | — | — | v1.1/v1.2 编译级，实测顺延补录 |

新平台移植步骤：实现基础四项契约 →（用 `ET_MODULE_KV` 时）再实现 flash 三件套与几何 → 以 `examples/stm32f103_demo.c` 为模板跑通最小 demo → 回填本表。

---

## 10. 配置项参考

全部位于 `et_config.h`，均带 `#ifndef` 保护，可用 `-D` 覆盖：

| 配置 | 默认 | 说明 |
|---|---|---|
| `ET_MODULE_RINGBUF / QUEUE / MEMPOOL / LIST / FILTER / STIMER / SCHED / EVENT / CRC / FRAME / ATCMD / KEY / LED / SPWM / KV / SOFTCLOCK / LOG` | 1 | 模块开关：置 0 后对应 `.c` 不参与编译（头文件内容亦被屏蔽） |
| `ET_RINGBUF_POW2` | 0 | 容量恒为 2 的幂时置 1（取模优化为位与） |
| `ET_MEMPOOL_ALIGN` | sizeof(void*) | 内存池块区对齐粒度 |
| `ET_MEMPOOL_STRICT` | 1 | free 时校验指针归属/重复释放 |
| `ET_SPWM_CH_MAX` | 4 | 软件 PWM 静态通道数 |
| `PORT_FLASH_SECTOR_SIZE` | 1024 | 参数区单扇区字节数（F103 页=1KB） |
| `PORT_FLASH_SECTOR_COUNT` | 16 | 参数区扇区数（et_kv 用其中两扇区） |
| `PORT_FLASH_ERASE_MS_MAX` | 20 | 单扇区擦除耗时上限（ms，喂狗参考） |
| `ET_VERSION_STRING / ET_VERSION` | "1.2.0" / 0x010200 | 版本标识，发布时须与 git tag 一致 |
| `ET_ASSERT(cond)` | 空实现 | 库内断言映射，可指向自身故障钩子 |
| `ET_LOG_MAX_LEVEL` | 0 (TRACE) | 日志编译期裁剪线（数值=最详细级别） |
| `ET_LOG_LINE_MAX` 等 | 见 et_log.h | 日志行为细节 |

---

## 11. 典型组合配方

### 11.1 裸机主循环骨架

```c
int main(void)
{
    hardware_init();
    app_modules_init();

    while (1) {
        uint32_t now = port_tick_get_ms();

        et_sched_poll_once();          /* 周期任务 */
        et_stimer_poll(now);           /* 定时器回调 */
        et_led_poll(&led, now);        /* 灯效刷新 */
        et_spwm_poll(now);             /* 软件 PWM 刷新 */
        et_key_scan(&key, now);        /* 按键扫描 */

        enter_sleep_until_interrupt(); /* WFI 低功耗可选 */
    }
}
```

### 11.2 UART 收包流水线（ISR → 主循环）

```c
/* ISR: 只做两件事 —— 入缓冲 + 置事件 */
void USART_IRQHandler(void) {
    while (USART->SR & RXNE) {
        uint8_t ch = USART->DR;
        et_ringbuf_write(&rb, &ch, 1);
    }
    et_event_set(&evt, EV_RX);
}

/* 主循环任务 */
void comm_task(void *arg) {
    (void)arg;
    if (et_event_wait_and_clear(&evt, EV_RX) & EV_RX) {
        uint8_t tmp[64]; uint32_t n;
        while ((n = et_ringbuf_read(&rb, tmp, sizeof(tmp))) > 0u) {
            et_frame_write(&parser, tmp, n);   /* 逐字节内部消化 */
        }
    }
}
```

### 11.3 帧 → 命令 二级解析

```c
static void on_frame(et_frame_parser_t *p, uint16_t len, void *user) {
    uint16_t i;
    for (i = 0; i < len; i++) {
        et_atcmd_feed(&at_proc, (char)p->rx_buf[i]);   /* 载荷即命令行 */
    }
    et_atcmd_feed(&at_proc, '\r');                     /* 结算 */
}
```

### 11.4 ISR → 任务 事件通知（代替信号量）

```c
/* ADC 转换完成中断 */
void ADC_IRQHandler(void) {
    last_sample = ADC->DR;
    et_event_set(&evt, EV_SAMPLE);
}

/* 采集任务(sched 5ms) */
void sample_task(void *arg) {
    (void)arg;
    if (et_event_wait_and_clear(&evt, EV_SAMPLE)) {
        filter_push(last_sample);
    }
}
```

### 11.5 无硬件 PWM 的 GPIO 呼吸灯（et_led → et_spwm 直连）

et_led 的亮度输出直接作为软件 PWM 占空比，`write` 回调一行直连：

```c
static et_led_t led;                                 /* spwm 按通道号使用, 无句柄 */

static void led_brightness_out(void *u, uint8_t v) { /* et_led 亮度 0~255 */
    (void)u;
    et_spwm_set(0, v);                               /* 直连: 亮度即占空比 */
}
static void ch0_out(void *u, uint8_t on) {           /* 软件 PWM 电平输出 */
    (void)u;
    gpio_write(LED_PIN, on);                         /* 低有效板载灯在此取反 */
}

et_spwm_init(0, ch0_out, NULL, 2);                   /* 500Hz */
et_led_init(&led, led_brightness_out, NULL);
et_led_set_breath(&led, 2000);

while (1) {
    uint32_t now = port_tick_get_ms();
    et_led_poll(&led, now);                          /* 亮度沿 → duty */
    et_spwm_poll(now);                               /* duty → 电平翻转 */
    ...
}
```

ADC 采样链同款思路：`et_movavg`（抑噪）→ `et_lpf1`（平滑）→ `et_slew`（防突变）级联，见 3.1。

---

> 更多实践参见 `examples/posix_demo.c`（全栈联动演示）、`examples/stm32f103_demo.c`（STM32 真机 demo）与 `test/` 下各模块单元测试——它们本身就是最好的用法范例。
