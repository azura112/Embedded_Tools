# port/_template —— 新平台移植脚手架 (v2.0 P3-2)

G474/F103 两平台移植经验的浓缩入口。复制本目录为 `port/<你的芯片>/` 后按
下文清单填空。**契约以 [`../port.h`](../port.h) 为唯一事实来源**（v2.0 冻结,
新增钩子走默认实现/弱符号 + `ET_VERSION` 门）。

## 契约四件套（必须实现）

| 组 | 函数 | 说明 |
|---|---|---|
| 时基 | `port_tick_get_ms()` | 1ms 单调递增毫秒时基（SysTick/OS 节拍皆可），允许自然回绕 |
| 输出 | `port_putc(char)` | 阻塞式单字符输出（日志/shell 共用） |
| 临界区 | `port_critical_enter/exit()` | 可嵌套计数（本库 host/F103/G474 实现同构），退出恢复原 PRIMASK |
| Flash 参数区 | `port_flash_read/write/erase_sector()` | 参数区 = flash 末端 `PORT_FLASH_SECTOR_COUNT × PORT_FLASH_SECTOR_SIZE`，offset 为参数区内偏移 |
| 看门狗(可选) | `port_wdt_enable/feed/disable()` | 不提供 wdt 用法可整组省略（`ET_MODULE_WDT=0`），但建议照抄骨架 |

## 几何宏清单（-D 或 et_config.h 覆盖）

| 宏 | 含义 | 例 |
|---|---|---|
| `PORT_FLASH_SECTOR_SIZE` | 参数区扇区(页)字节数 | F1: 1024 / G4: 2048 |
| `PORT_FLASH_SECTOR_COUNT` | 参数区扇区数（≥4，kv 2 + bootctl 3） | 16 |
| `PORT_FLASH_ERASE_MS_MAX` | 单扇区擦除耗时上限 ms（wdt 超时下限 = 2×此值） | F1: 20 / G4: 40 |

## 移植 checklist（顺序即验收顺序）

1. 填空 `port_<芯片>.c`（骨架在 `port_template.c`，TODO 处实现）；
2. **加 `#error` 几何守卫**（骨架已含样例：三宏缺失/取值错 → 编译失败，防"忘了 -D 静默错配"——v1.6 G4 教训）；
3. 启动文件 + 链接脚本：把参数区从代码区挖走并 `ASSERT` 兜底（参 `port/stm32f103/stm32f103c8t6.ld`）；
4. **双几何回归硬性项**：在 host 侧以本平台几何宏跑 `make test` 同款全量（布局类假设在这里暴露，参 v1.8 8B 槽教训）；
5. 零警告交叉编译：`-Wall -Wextra -pedantic` 无输出；
6. `README.md` 写构建命令并用 ` ```docbuild ` 定界（CI `tools/docbuild.sh` 原样执行——文档即测试）,
   体积表加当前版本行后跑 `sh tools/sizecheck.sh`（若纳入常态 CI 门再扩展 port 列表）;
7. flash 契约自测：write 后立即 read 比对、erase 后全 0xFF、双字/单字平台注意**编程粒度与
   重复编程限制**（G4 64 位双字单次编程教训，写进 README 差异节）；
8. 上板最小 demo（参考 `examples/stm32g474_demo.c`：横幅/kv 重启计数/RX 中断+环/tickless
   ——**WFI 必配 RX 中断唤醒**，配方 11.8）。

## 常见坑速查

- **WFI + 轮询 RX = 结构性丢包**（115200 字节间隔 87µs < 1ms 睡眠窗口）：中断 + `et_ringbuf`；
- flash 非全 1 目标再编程（G4 PROGERR）：布局代码必须双几何回归；
- 擦除窗口内看门狗饿死：`et_wdt` 契约下限 `2×ERASE_MS_MAX` 已内建，勿绕过；
- 临界区嵌套：退出必须恢复到**进入前**的 PRIMASK（计数法），简单 set/restore-0 会吞外层。
