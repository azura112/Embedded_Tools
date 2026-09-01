# Embedded_Tools

一套面向嵌入式 MCU 的 C99 组件库：**零动态内存、多实例句柄化、分层单向依赖、PC 可全量单测**。

> 当前版本：**v1.1.0**（`ET_VERSION_STRING`）｜ 版本路线与变更记录见 **[v1.1开发计划：平台移植与新模块.md](v1.1开发计划：平台移植与新模块.md)**

> 📖 完整接口手册见 **[docs/API_GUIDE.md](docs/API_GUIDE.md)**（每个 API 的签名、并发约束与示例）

## 特性总览

| 层 | 模块 | 说明 |
|---|---|---|
| core/ | `et_ringbuf` | SPSC 无锁环形缓冲，自由递增索引抗回绕，零拷贝连续段 API |
|  | `et_queue` | 定长消息队列（同款无锁技巧） |
|  | `et_mempool` | 固定块内存池，位图管理，STRICT 防重复释放 |
|  | `et_list` | 侵入式双向链表，O(1) 插删，遍历中自删安全 |
| algorithm/ | `et_filter` | 定点滤波器组：滑动均值 / Q15 一阶低通 / 斜率限制（纯算法层，禁 port.h） |
| sys/ | `et_stimer` | 软件定时器，ISR 可启停，周期追赶语义 |
|  | `et_sched` | 协作式周期任务调度器（主循环轮询） |
|  | `et_event` | 32 位事件标志组（ISR 置位/主循环消费） |
| protocol/ | `et_crc` | CRC8/CRC16-MODBUS/CCITT/CRC32，流式增量 API |
|  | `et_frame` | 字节流帧解析状态机，协议格式可配置，配套组帧函数 |
|  | `et_atcmd` | 行式 AT 命令解析器 |
| drivers/ | `et_key` | 按键四态 FSM：消抖/短按/长按/连发 |
|  | `et_led` | LED 模式管理：常亮/闪烁N次/呼吸，输出缓存 |
|  | `et_spwm` | 多通道软件 PWM（1ms 时基相位法，≤500Hz） |
| debug/ | `et_log` | 分级日志：运行时过滤 + 编译期裁剪 + 自带格式化器 + hexdump |
|  | `et_assert` | 断言：失败钩子可插拔（记录/停机/复位） |

## 目录结构

```
├── et_config.h        # 全局裁剪配置 + 版本宏
├── core/ algorithm/ sys/ protocol/ drivers/ debug/
├── port/
│   ├── port.h         # 平台适配契约（唯一碰硬件的层）
│   ├── host/          # PC 模拟实现（含虚拟时间注入，测试用）
│   └── stm32f103/     # STM32F103 真机移植（启动代码/链接脚本/最小寄存器头）
├── test/              # 迷你框架 + 150 个单元用例
├── examples/
│   ├── posix_demo.c   # 全栈联动演示
│   └── stm32f103_demo.c   # BluePill 真机 demo（blink/按键/呼吸灯/日志）
└── Makefile
```

## 快速开始

```sh
make test    # Windows 无 make 环境用 mingw32-make, 或直接:
gcc -std=c99 -Wall -Wextra -pedantic -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Iport -Iport/host \
    -o build/et_tests.exe core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c \
    port/host/port_host.c test/*.c
./build/et_tests.exe

make demo    # 运行全栈演示（虚拟 UART 链路 + 定时器 + 按键 + LED）
```

STM32F103 真机构建/烧录见 [port/stm32f103/README.md](port/stm32f103/README.md)。

## 使用速查

**环形缓冲（UART 收发标配）**
```c
static et_ringbuf_t rb; static uint8_t mem[256];
et_ringbuf_init(&rb, mem, sizeof(mem));

void USART1_IRQHandler(void) {                 /* ISR 侧只管写 */
    et_ringbuf_write(&rb, &rx_byte, 1);
}
/* 主循环侧读取 */
uint8_t buf[32]; uint32_t n = et_ringbuf_read(&rb, buf, sizeof(buf));
```

**软定时器 + 调度器（前后台骨架）**
```c
et_task_t t_led; et_stimer_t hb;
et_sched_register(&t_led, led_task, NULL, 10);      /* 10ms 周期任务 */
et_stimer_start_periodic(&hb, heartbeat_fn, 500);   /* 半秒心跳 */
while (1) {
    uint32_t now = port_tick_get_ms();
    et_sched_poll_once();                            /* 到期任务执行 */
    et_stimer_poll(now);                             /* 到期回调分发 */
}
```

**帧解析（逐字节喂入，ISR 友好）**
```c
et_frame_cfg_t cfg = {
    .header = (const uint8_t[]){0xAA,0x55}, .header_len = 2,
    .use_len = true, .crc = ET_FRAME_CRC_CRC16_MODBUS,
    .rx_buf = pay_buf, .rx_cap = sizeof(pay_buf),
    .on_frame = my_frame_handler,
};
et_frame_parser_init(&parser, &cfg);
/* 收到一个字节就喂一个 */
et_frame_feed(&parser, ch_from_uart);
```

**侵入式链表（零拷贝组织结构体）**
```c
typedef struct { et_list_node_t node; int id; } item_t;   /* 节点嵌入用户结构 */
et_list_push_back(&list, &item->node);
et_list_remove(&list, &item->node);                       /* O(1) */
et_list_foreach(&list, visit_fn, user);                   /* 遍历中自删安全 */
```

**GPIO 呼吸灯（et_led → et_spwm 直连，无硬件 PWM 板）**
```c
et_spwm_init(0, led_gpio_write, NULL, 2);     /* 500Hz 软件 PWM */
et_led_init(&led, (et_led_write_fn)led_to_spwm, NULL);
et_led_set_breath(&led, 2000);                /* write 回调内 et_spwm_set(0, v) */
```

## 移植指南

只需实现 `port/port.h` 契约：

| 接口 | 要求 |
|---|---|
| `PORT_CRITICAL_ENTER/EXIT` | 可嵌套临界区（如 Cortex-M 的 PRIMASK 保存恢复） |
| `port_tick_get_ms()` | 毫秒单调时基（SysTick 等），允许自然回绕 |
| `port_putc()` | 阻塞式字符输出（日志底层） |

已验证平台：host（CI 双系统全量单测）、**STM32F103C8T6**（`port/stm32f103/`，零警告编译，实测记录见其 README）。

裁剪：编辑 `et_config.h` 中 `ET_MODULE_*` 开关（支持 `-D` 覆盖），未启用的模块不参与编译。

## 设计原则

- **零动态内存**：所有实例由调用方分配，库内无 malloc；
- **多实例句柄化**：一切经 `et_xxx_t*` 操作，无隐藏全局状态（stimer 注册表除外，已文档化）；
- **并发策略显式声明**：每个头文件标明 ISR-safe 范围与所属上下文限制；
- **单向依赖**：core ← sys ← drivers，debug/protocol 独立，硬件仅存在于 port 层；
- **PC 可测**：核心逻辑纯逻辑化，host port 提供虚拟时间注入，150 用例覆盖回绕/并发边界/畸形输入/定点数值。

## 测试策略亮点

- 白盒构造 uint32 索引回绕点验证环形缓冲数学正确性；
- SPSC 单字节流压测（2 万次）+ 多字节块粒度压测（4 万次）双覆盖——后者曾揪出跨回绕拷贝的剩余段长度 bug；
- CRC 与标准校验向量比对（`"123456789"` → F4/4B37/29B1/CBF43926）；
- 帧解析器抗噪/坏帧/超长注入后自动重同步；
- 日志输出捕获做精确字符串断言。
