# 从零到板上 —— Embedded_Tools 快速上手 (v2.1)

> 目标: 用**一条命令一步**的节奏，把库裁成自己的、跑通 host 单测、移植到新 MCU、上板看串口。
> 全程 C99、零动态内存; 每步都有可复现命令与"该看到什么"。
> 相关深读: 接口细节 [API_GUIDE.md](API_GUIDE.md) ｜ 平台移植 [port/_template/README.md](../port/_template/README.md) ｜ 发布纪律 [README](../README.md) checklist。

---

## 0. 环境准备 (一次性)

| 用途 | 工具 | 验证命令 |
|---|---|---|
| host 单测 | gcc (C99) | `gcc --version` |
| ARM 交叉 | GNU Tools for STM32 13.3.rel1 | `arm-none-eabi-gcc --version` |
| 烧录 | STM32CubeProgrammer CLI + ST-LINK | `STM32_Programmer_CLI --version` |
| (可选) 构建 | MinGW-w64 的 `mingw32-make` | `mingw32-make --version` |

Windows Git Bash 下没有 `make` 也能用 `mingw32-make`; 或直接照 README 的纯 gcc 命令。

## 1. 裁剪: 只留你要的模块

```sh
# 编辑 et_config.h, 把不用的 ET_MODULE_* 置 0 (或用 -D 覆盖, 不动源文件):
gcc -DET_MODULE_SELFTEST=1 -DET_MODULE_KV=1 ...   # 例: 只启用自测与 KV
```

**规则**: 开关置 0 后对应 `.c` 移出构建列表(头文件内容亦被屏蔽)。最小系统 = 你实际调用的模块;
`ET_MODULE_SELFTEST` 默认 0(发布裁剪)，host 测试与板上自测固件用 `-DET_MODULE_SELFTEST=1` 打开。

**该看到什么**: 编译源清单里不再出现被裁掉模块的 `.c`; 引用被裁模块的代码会编译失败——这是有意的
（防止"关了开关却漏删调用"的静默错配）。

## 2. host 单测: 先证明算法对，再谈硬件

```sh
cd Embedded_Tools
mingw32-make test        # 或 make test
```

**该看到什么**: 末尾 `=== RESULT: ALL PASS (fail=0) ===`，共 **398** 例。

```sh
mingw32-make test-g4     # 双几何回归 (G474 2KB 页几何; storage 布局改动必跑)
mingw32-make test-tab    # shell Tab 补全形态 (ET_SHELL_TAB=1) 独立矩阵
mingw32-make bench       # host 基准 (数字入 docs/bench.md, 须附环境注记)
```

没有 `make` 时，直接照 [README](../README.md) "快速开始" 里的纯 gcc 命令复制粘贴即可。

## 3. 移植到你的 MCU: 填 port 契约

库只有一个硬件边界——`port/port.h`。复制模板目录开始:

```sh
cp -r port/_template port/stm32f103        # 换成你的平台名
# 按 port/_template/README.md 填空: port_tick_get_ms / port_putc /
#   port_critical_enter·exit / port_flash_read·write·erase_sector / (可选) port_wdt_*
```

**几何三宏**（flash 参数区）必须按芯片覆盖，缺了会被 `#error` 守卫挡住(防"忘了 -D 静默错配"):

```sh
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c99 -Wall -Wextra -pedantic -Os \
  -DPORT_FLASH_SECTOR_SIZE=2048 -DPORT_FLASH_SECTOR_COUNT=16 -DPORT_FLASH_ERASE_MS_MAX=40 \
  -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/stm32g474 \
  -c port/stm32g474/port_stm32g474.c -o /dev/null
```

**该看到什么**: 零警告通过; 去掉任一 `-D` 立即 `#error` 失败(正是守卫在起作用)。

**移植验收顺序**(摘自模板 checklist): 填空 → 几何守卫 → 链接脚本挖走参数区 + `ASSERT` 兜底 →
host 侧以本平台几何跑 `make test` → 零警告交叉编译 → README 写 ` ```docbuild ` 定界构建块 →
flash 契约自测 → 上板最小 demo。完整清单见 [port/_template/README.md](../port/_template/README.md)。

## 4. 串口调试: shell + AT 命令

裸机 demo 的常规骨架: **UART RX 中断写 `et_ringbuf`**（不要在睡眠窗口里轮询 RX —— 115200 下字节间隔
87µs < 1ms 睡眠窗口，必丢包，见 API_GUIDE 11.8），主循环排空环 → `et_atcmd`/`et_shell` 解析。

```c
static et_ringbuf_t rx_rb;  static uint8_t rx_mem[128];
et_ringbuf_init(&rx_rb, rx_mem, sizeof(rx_mem));

void USART1_IRQHandler(void) {                  /* ISR: 只管写 */
    uint8_t ch = (uint8_t)(USART1->DR & 0xFFu);
    (void)et_ringbuf_write(&rx_rb, &ch, 1u);
}

/* 主循环: 排空环 -> 喂 shell (atcmd 之上, 自带回显/help) */
uint8_t b;
while (et_ringbuf_read(&rx_rb, &b, 1u) == 1u) {
    (void)et_shell_feed(&sh, b);
}
```

串口助手接 115200-8N1，敲 `help` 回车。

**该看到什么**: 命令行回显 + `help` 列出你注册的 AT 命令; 未注册命令返回错误而非死机。

## 5. 板上自测: 一条命令冒烟

在 `et_config.h`（或 `-D`）打开 `ET_MODULE_SELFTEST=1`，固件里跑一遍内建套件，报告经串口回读。

```c
#if ET_MODULE_SELFTEST
et_selftest_run_all(my_report_cb, NULL);        /* 20 套件: ringbuf/filter/pid/... kv/bootctl */
#endif
```

或挂到 AT 命令上（参考 F103/G474 demo 的 `AT+SELFTEST`）。

**该看到什么**: 逐套件 `PASS` 与末行 `SELFTEST: 20/20`（`ET_MODULE_KV=1` 且已 init 时才跑 kv/bootctl
破坏性套件）。host 侧同一组件会复跑一遍，板上/PC 结果可直接比对。

## 6. 闭环控制示例 (v2.1 新模块)

"测量 → 滤波 → 控制 → 输出"四段齐了: `et_filter` → `et_pid` → `et_spwm`，`et_stats` 旁路判稳。

```c
et_lpf1_init(&lpf, 8192u);                                  /* 去噪(微分前置滤波) */
et_pid_cfg_t cfg = { .kp=19660, .ki=4915, .kd=1640,
                     .out_min=0, .out_max=1000, .i_min=-300, .i_max=300,
                     .d_on_measure=1 };
et_pid_init(&pid, &cfg);

/* 固定 10ms 周期任务: */
int32_t pv  = et_lpf1_update(&lpf, adc_read());
int32_t out = et_pid_step(&pid, 500, pv, 10u);
et_spwm_set(0u, (uint32_t)out);
et_stats_push(&st, pv);
```

整定步骤（先 P 后 I 再 D、Ziegler-Nichols 定性、抗饱和与常见坑）见
[API_GUIDE 11.10](API_GUIDE.md#1110-定点闭环整定配方et_lpf1--et_pid--et_spwm--et_statsv21)。

## 7. 提交前自查 (与发布 checklist 对齐)

```sh
sh tools/docsync.sh              # 文档同步断言 (含 apidump --check)
sh tools/apidump.sh --diff       # 公开面 vs v2.0 冻结基线: 必须"纯新增"
sh tools/sizecheck.sh            # ARM 体积表与实测逐值比对 (需 arm-none-eabi-size)
```

**该看到什么**: `docsync: pass=N fail=0`; `apidump: OK —— 纯新增`; `sizecheck: pass=3 fail=0`。

---

## 排障三分法 (板上异常时)

1. **库侧**: 先在 host 跑 `make test` 复现——能复现就是库问题（补一条单测再修）;
2. **板侧**: 零警告交叉编译 + `AT+SELFTEST` 逐套件定位; 检查时钟/时基与 flash 几何三宏;
3. **接线/外设**: UART 波特率与地线、LED 极性、IWDG 窗口——三分法之外的问题多半在这里。

记录模板与真实案例见仓库根 `移植stm32实机记录.md`（G474 串口丢包、IWDG 复位循环、升级链走单）。
