# Embedded_Tools API 指南

> 适用版本：v1.0 ｜ 语言标准：C99 ｜ 目标环境：裸机前后台循环（兼容任意 MCU）

---

## 目录

- [1. 全局约定](#1-全局约定)
- [2. core 数据结构层](#2-core-数据结构层)
  - [2.1 et_ringbuf 环形缓冲区](#21-et_ringbuf-环形缓冲区)
  - [2.2 et_queue 定长消息队列](#22-et_queue-定长消息队列)
  - [2.3 et_mempool 固定块内存池](#23-et_mempool-固定块内存池)
- [3. sys 系统服务层](#3-sys-系统服务层)
  - [3.1 et_stimer 软件定时器](#31-et_stimer-软件定时器)
  - [3.2 et_sched 任务调度器](#32-et_sched-任务调度器)
  - [3.3 et_event 事件标志组](#33-et_event-事件标志组)
- [4. protocol 协议层](#4-protocol-协议层)
  - [4.1 et_crc 校验计算](#41-et_crc-校验计算)
  - [4.2 et_frame 帧解析器](#42-et_frame-帧解析器)
  - [4.3 et_atcmd 命令解析器](#43-et_atcmd-命令解析器)
- [5. drivers 设备驱动层](#5-drivers-设备驱动层)
  - [5.1 et_key 按键](#51-et_key-按键)
  - [5.2 et_led LED](#52-et_led-led)
- [6. debug 调试层](#6-debug-调试层)
  - [6.1 et_log 日志](#61-et_log-日志)
  - [6.2 et_assert 断言](#62-et_assert-断言)
- [7. port 平台适配契约](#7-port-平台适配契约)
- [8. 配置项参考](#8-配置项参考)
- [9. 典型组合配方](#9-典型组合配方)

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
-I. -Icore -Isys -Iprotocol -Idrivers -Idebug -Iport
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

## 3. sys 系统服务层

三个模块共享同一个毫秒时基（来自 `port_tick_get_ms()`），互不冲突，可按需选用。

### 3.1 et_stimer 软件定时器

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

### 3.2 et_sched 任务调度器

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

### 3.3 et_event 事件标志组

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

---

## 4. protocol 协议层

### 4.1 et_crc 校验计算

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

### 4.2 et_frame 帧解析器

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

### 4.3 et_atcmd 命令解析器

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

/* 完帧回调里接力(见 9.3 组合配方) */
char ch;
while (get_char(&ch)) et_atcmd_feed(&at, ch);
```

---

## 5. drivers 设备驱动层

两个驱动均通过回调抽象硬件（电平读取 / 亮度写出），不直接依赖 port GPIO——矩阵键盘、IO 扩展器、软件 PWM 均可接入。

### 5.1 et_key 按键

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

### 5.2 et_led LED

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

---

## 6. debug 调试层

### 6.1 et_log 日志

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

### 6.2 et_assert 断言

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

## 7. port 平台适配契约

移植整个库只需实现 `port/port.h` 声明的四个能力（参考实现见 `port/host/`）：

| 接口 | 要求 | 典型实现 |
|---|---|---|
| `PORT_CRITICAL_ENTER() / EXIT()` | 可嵌套临界区，保护主循环与 ISR 共享数据 | Cortex-M: 保存/恢复 PRIMASK + CPSID/CPSIE |
| `port_critical_enter()/exit()` | 同上（宏的函数形态） | 同上 |
| `port_tick_get_ms()` | 毫秒单调时基，由中断维护，允许自然回绕 | SysTick 中断累加 |
| `port_putc(c)` | 阻塞式单字符输出（日志底层） | USART 轮询发送 |

分层纪律：**core 层禁止包含 port.h**（保证纯逻辑可 PC 测试）；sys/drivers/debug 仅经此契约触硬件。

---

## 8. 配置项参考

全部位于 `et_config.h`，均带 `#ifndef` 保护，可用 `-D` 覆盖：

| 配置 | 默认 | 说明 |
|---|---|---|
| `ET_MODULE_RINGBUF / QUEUE / MEMPOOL / STIMER / SCHED / EVENT / CRC / FRAME / ATCMD / KEY / LED / LOG` | 1 | 模块开关：置 0 后对应 `.c` 不参与编译 |
| `ET_RINGBUF_POW2` | 0 | 容量恒为 2 的幂时置 1（取模优化为位与） |
| `ET_MEMPOOL_ALIGN` | sizeof(void*) | 内存池块区对齐粒度 |
| `ET_MEMPOOL_STRICT` | 1 | free 时校验指针归属/重复释放 |
| `ET_ASSERT(cond)` | 空实现 | 库内断言映射，可指向自身故障钩子 |
| `ET_LOG_MAX_LEVEL` | 0 (TRACE) | 日志编译期裁剪线（数值=最详细级别） |
| `ET_LOG_LINE_MAX` 等 | 见 et_log.h | 日志行为细节 |

---

## 9. 典型组合配方

### 9.1 裸机主循环骨架

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
        et_key_scan(&key, now);        /* 按键扫描 */

        enter_sleep_until_interrupt(); /* WFI 低功耗可选 */
    }
}
```

### 9.2 UART 收包流水线（ISR → 主循环）

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

### 9.3 帧 → 命令 二级解析

```c
static void on_frame(et_frame_parser_t *p, uint16_t len, void *user) {
    uint16_t i;
    for (i = 0; i < len; i++) {
        et_atcmd_feed(&at_proc, (char)p->rx_buf[i]);   /* 载荷即命令行 */
    }
    et_atcmd_feed(&at_proc, '\r');                     /* 结算 */
}
```

### 9.4 ISR → 任务 事件通知（代替信号量）

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

---

> 更多实践参见 `examples/posix_demo.c`（全栈联动演示）与 `test/` 下各模块单元测试——它们本身就是最好的用法范例。
