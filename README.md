# Embedded_Tools

一套面向嵌入式 MCU 的 C99 组件库：**零动态内存、多实例句柄化、分层单向依赖、PC 可全量单测**。

> 当前版本：**v1.2.0**（`ET_VERSION_STRING`）｜ 版本路线与变更记录见 **[v1.2开发计划：et_kv存储与实测补录.md](v1.2开发计划：et_kv存储与实测补录.md)**

> 📖 完整接口手册见 **[docs/API_GUIDE.md](docs/API_GUIDE.md)**（每个 API 的签名、并发约束与示例）

## 特性总览

| 层 | 模块 | 说明 |
|---|---|---|
| core/ | `et_ringbuf` | SPSC 无锁环形缓冲，自动递增索引抗回绕，零拷贝连续段 API |
|  | `et_queue` | 定长消息队列（同款无锁技巧） |
|  | `et_mempool` | 固定块内存池，位图管理，STRICT 防重复释放 |
|  | `et_list` | 侵入式双向链表，O(1) 插删，遍历中自删安全 |
| algorithm/ | `et_filter` | 定点滤波器组：滑动均值 / Q15 一阶低通 / 斜率限制（纯算法层，禁 port.h） |
| sys/ | `et_stimer` | 软件定时器，ISR 可启停，周期追赶语义 |
|  | `et_sched` | 协作式周期任务调度器（主循环轮询） |
|  | `et_event` | 32 位事件标志组（ISR 置位/主循环消费） |
|  | `et_softclock` | 软时钟：ms tick → UTC 日历（Hinnant 整数算法，1970–2106） |
| storage/ | `et_kv` | flash 键值掉电存储：双扇区乒乓 + 逐条 CRC + 断电自愈 |
| protocol/ | `et_crc` | CRC8/CRC16-MODBUS/CCITT/CRC32，流式增量 API |
|  | `et_frame` | 字节流帧解析状态机，协议格式可配置，配套组帧函数 |
|  | `et_atcmd` | 行式 AT 命令解析器 |
| drivers/ | `et_key` | 按键四态 FSM：消抖/短按/长按/连发 |
|  | `et_led` | LED 模式管理：常亮/闪烁 N 次/呼吸，输出缓存 |
|  | `et_spwm` | 多通道软件 PWM（1ms 时基相位法，≤500Hz） |
| debug/ | `et_log` | 分级日志：运行时过滤 + 编译期裁剪 + 自带格式化器 + hexdump |
|  | `et_assert` | 断言：失败钩子可插拔（记录/停机/复位） |

## 目录结构

```
├── et_config.h        # 全局裁剪配置 + 版本宏（含 flash 参数区几何）
├── core/ algorithm/ sys/ storage/ protocol/ drivers/ debug/
├── port/
│   ├── port.h         # 平台适配契约（唯一碰硬件的层，v1.2 起含 flash 三件套）
│   ├── host/          # PC 模拟实现（flash 模拟器 + 时间注入 + 掉电注入）
│   └── stm32f103/     # STM32F103 真机移植（FLASH 驱动/启动代码/链接脚本）
├── test/              # 迷你框架 + 191 个单元用例（kv 断电恢复矩阵 28 例）
├── examples/
│   ├── posix_demo.c   # 全栈联动演示
│   └── stm32f103_demo.c   # BluePill 真机 demo（blink/按键/呼吸灯/重启计数/软时钟）
└── Makefile
```

## 快速开始

```sh
make test    # Windows 无 make 环境用 mingw32-make；或直接：
gcc -std=c99 -Wall -Wextra -pedantic -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/host \
    -o build/et_tests.exe core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c storage/*.c \
    port/host/port_host.c test/*.c
./build/et_tests.exe

make demo    # 全栈演示（虚拟 UART + 定时器 + 按键 + LED + kv）
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
uint8_t buf[32]; uint32_t n = et_ringbuf_read(&rb, buf, sizeof(buf)); /* 主循环读 */
```

**软定时器 + 调度器（前后台骨架）**
```c
et_task_t t_led; et_stimer_t hb;
et_sched_register(&t_led, led_task, NULL, 10);      /* 10ms 周期任务 */
et_stimer_start_periodic(&hb, heartbeat_fn, NULL, 500);   /* 半秒心跳 */
while (1) {
    uint32_t now = port_tick_get_ms();
    et_sched_poll_once();  et_stimer_poll(now);
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
et_frame_parser_init(&p, &cfg);
et_frame_feed(&p, ch_from_uart);
```

**GPIO 呼吸灯（et_led → et_spwm 直连，无硬件 PWM 板）**
```c
et_spwm_init(0, led_gpio_write, NULL, 2);   /* 500Hz */
et_led_init(&led, (et_led_write_fn)led_to_spwm, NULL);
et_led_set_breath(&led, 2000);              /* write 回调内 et_spwm_set(0, v) */
```

**掉电参数保存（et_kv，重启计数即插即用）**
```c
et_kv_t kv;  et_kv_layout_t lay = { 14u, 15u };   /* 参数区内两扇区 */
if (!et_kv_init(&kv, &lay)) { et_kv_format(&kv, &lay); (void)et_kv_init(&kv, &lay); }
uint32_t n = 0u;
et_kv_get(&kv, 1, &n, sizeof(n), NULL);  n++;
et_kv_set(&kv, 1, &n, sizeof(n));          /* 即时持久化，断电自愈 */
```

## 移植指南

只需实现 `port/port.h` 契约：

| 接口 | 要求 |
|---|---|
| `PORT_CRITICAL_ENTER / EXIT` | 可嵌套临界区（如 Cortex-M PRIMASK 保存恢复） |
| `port_tick_get_ms()` | 毫秒单调时基（SysTick 等），允许自然回绕 |
| `port_putc()` | 阻塞式字符输出（日志底层） |
| `port_flash_read/write/erase_sector` | 仅 `ET_MODULE_KV=1` 时必选：4B 对齐擦写、只允许 1→0 写、短写如实上报（掉电/故障截断） |

已验证平台：host（CI 双平台全量测试）、**STM32F103C8T6**（`port/stm32f103/`，零警告编译 + 片内 flash 参数区，真机记录见其 README）。

裁剪：编辑 `et_config.h` 中 `ET_MODULE_*` 开关（支持 `-D` 覆盖），未启用的模块不参与编译（对应 `.c` 亦移出构建列表）。

## 设计原则

- **零动态内存**：所有实例由调用方分配，库内无 malloc；
- **多实例句柄化**：一切经 `et_xxx_t*` 操作，无隐藏全局状态（stimer 注册表除外，已文档化）；
- **并发策略显式声明**：每个头文件标明 ISR-safe 范围与所属上下文限制；
- **单向依赖**：core/algorithm ← sys ← storage/drivers ← port，硬件仅存在于 port 层；
- **PC 可测**：核心逻辑纯算法化，host port 提供虚拟 flash（含掉电截断注入）+ 时间注入，191 用例覆盖回绕/并发边界/畸形输入/掉电恢复/定点数值。

## 测试与质量门

- **单元测试**：迷你框架，双平台主机全量运行，ALL PASS（191 例）；
- **掉电恢复矩阵**：kv 页头/记录/压缩断点每类 ≥2 注入点，掉电后重开全部恢复；
- **CI 门控**（`.github/workflows/ci.yml`）：host 测试 × 覆盖率 gcovr 行覆盖 ≥85%（实测 96.9%）× ARM 零警告交叉编译；
- **发布**（`.github/workflows/release.yml`）：`v*` tag → 全量测试 + ARM ELF/BIN → GitHub Release 附件。

## 测试策略亮点

- 白盒构造 uint32 索引回绕点验证环形缓冲数学正确性；
- SPSC 单字节流压测（2 万次）+ 多字节块粒度压测（4 万次）双覆盖——后者曾揪出跨回绕拷贝剩余段 bug；
- CRC 与标准校验向量比对（`"123456789"` → F4/4B37/29B1/CBF43926）；
- 帧解析器抗噪/坏帧/超长注入后自动重同步；
- 日志输出捕获做精确字符串断言。

---

## 开发者发布 checklist（v1.3 起，防"声称与实际脱节"）

发布/交付文档中每一项"已完成"声明必须可验证。打 tag（触发 `release.yml`）前逐条核对：

1. **文档同步逐文件核对**：`README.md`、`docs/API_GUIDE.md`、`Makefile`、`port/*/README.md` —— 用 `grep` 确认版本号/新模块名/新目录均已出现在对应文件（例：`grep -c et_kv README.md` 非 0），不能只改一处；
2. **版本钉三方一致**：`et_config.h` 的 `ET_VERSION_STRING` = git tag = 交付文档版本行（`gcc -E -dM et_config.h | grep ET_VERSION_STRING` 复现）；
3. **量化声明附复现命令**：用例数（`make test` 输出 RESULT 行）、覆盖率（`gcovr --print-summary`）、ARM 体积（`arm-none-eabi-size`）、零警告（`-Wall -Wextra -pedantic` 下无输出）；
4. **主机回归全绿**：`make test` 本地跑一遍后再打 tag，不以"CI 会跑"替代本地验证；
5. **交付文档命名**：`v<版本号>开发交付：<重点概况>.md`（全角冒号），里程碑对照提交哈希逐条可回溯。