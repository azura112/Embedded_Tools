#!/bin/sh
# =====================================================================
# docsync.sh —— 文档同步断言 (v1.5 P0-3 根治条款)
#
# 背景: v1.2 与 v1.4 两次发生"交付文档声称已同步, 实际未同步"的脱节,
# 人肉 checklist 防不住 —— 本脚本把"已同步"声明固化为可执行断言:
#   - 每条断言 = 文件 + grep 模式 + 一句话说明;
#   - 发布 checklist (README) 与 CI (ci.yml unit-tests job) 均调用;
#   - DoD 硬性条款: 任何"文档已同步"声明必须对应本脚本内断言,
#     无断言的声明视为未同步。
#
# 用法: sh tools/docsync.sh            # 全绿退出 0, 任一失败退出 1
# 自证: 临时破坏任一断言目标文件内容后重跑应变红 (交付验收项)
# =====================================================================
set -u

FAIL=0
PASS=0

# 断言主体: assert_grep <文件> <扩展grep模式> <说明>
assert_grep() {
    file="$1"; pat="$2"; desc="$3"
    if [ ! -f "$file" ]; then
        echo "FAIL [文件缺失] $desc ($file)"
        FAIL=$((FAIL + 1))
        return
    fi
    if grep -Eq -- "$pat" "$file"; then
        echo "ok   $desc"
        PASS=$((PASS + 1))
    else
        echo "FAIL $desc  (文件 $file 中未命中: $pat)"
        FAIL=$((FAIL + 1))
    fi
}

# ---- 版本链一致性 (动态取 et_config 展开值, 不硬编码) ----
V=$(gcc -E -P -dM et_config.h 2>/dev/null | awk '/define ET_VERSION_MAJOR/{maj=$3}
                                                  /define ET_VERSION_MINOR/{min=$3}
                                                  /define ET_VERSION_PATCH/{pat=$3}
                                                  END{print maj"."min"."pat}')
if [ -z "$V" ]; then
    echo "FAIL 无法从 et_config.h 解析版本号 (需要 gcc 预处理器)"
    FAIL=$((FAIL + 1))
else
    assert_grep "README.md"          "当前版本：\\*\\*v$V\\*\\*" "README 版本行与 ET_VERSION_STRING($V) 一致"
    assert_grep "docs/API_GUIDE.md"  "适用版本：v$V"             "API_GUIDE 适用版本与版本链一致"
    # 整数编码与版本分量一致 (例: 1.5.0 -> 0x010500)
    HEX=$(printf "0x%02X%02X%02X" $(echo "$V" | tr '.' ' '))
    assert_grep "et_config.h"        "$HEX"              "et_config 整数编码 $HEX 与版本 $V 一致"
fi

# ---- v1.4 验收缺口两条 (本批根治的直接动因) ----
assert_grep "port/stm32f103/README.md" "12276"                                  "port README 体积表含 v1.4 行 (text=12276)"
assert_grep "port/stm32f103/README.md" "GNU Tools for STM32"                    "port README 体积测量环境注记 (工具链版本)"

# ---- 体积表逐版本可追溯 (v1.1~v1.4) ----
assert_grep "port/stm32f103/README.md" "7976"    "体积表 v1.1 行"
assert_grep "port/stm32f103/README.md" "10892"   "体积表 v1.2 行"
assert_grep "port/stm32f103/README.md" "11208"   "体积表 v1.3 行"

# ---- 构建脚本与源清单同步 ----
assert_grep "Makefile" "protocol/et_xmodem.c" "Makefile 含 et_xmodem"
assert_grep "Makefile" "debug/et_shell.c"     "Makefile 含 et_shell"
assert_grep "Makefile" "algorithm/et_fsm.c"   "Makefile 含 et_fsm"
assert_grep "Makefile" "sys/et_softclock.c"   "Makefile 含 et_softclock"
assert_grep "Makefile" "storage/et_kv.c"      "Makefile 含 et_kv"

# ---- 测试套件注册 ----
assert_grep "test/test_main.c" "test_xmodem_cases"  "test_main 注册 xmodem 套件"
assert_grep "test/test_main.c" "test_shell_cases"   "test_main 注册 shell 套件"
assert_grep "test/test_main.c" "test_bootctl_cases" "test_main 注册 bootctl 套件"
assert_grep "test/test_main.c" "test_wdt_cases"     "test_main 注册 wdt 套件"
assert_grep "test/test_main.c" "test_fsm_cases"     "test_main 注册 fsm 套件"
assert_grep "test/test_main.c" "test_kv_cases"      "test_main 注册 kv 套件"

# ---- README 模块行 (v1.0~v1.4 已交付模块) ----
assert_grep "README.md" "et_ringbuf"   "README 特性表: ringbuf"
assert_grep "README.md" "et_softclock" "README 特性表: softclock"
assert_grep "README.md" "et_kv"        "README 特性表: kv"
assert_grep "README.md" "et_fsm"       "README 特性表: fsm"
assert_grep "README.md" "et_xmodem"    "README 特性表: xmodem"
assert_grep "README.md" "et_shell"     "README 特性表: shell"
assert_grep "README.md" "et_bootctl"   "README 特性表: bootctl"
assert_grep "README.md" "et_wdt"       "README 特性表: wdt"
assert_grep "README.md" "et_kv_iter"   "README 速查/正文: kv_iter"

# ---- API_GUIDE 章节锚点 (v1.2~v1.4 新章) ----
assert_grep "docs/API_GUIDE.md" "6.1 et_kv"              "API_GUIDE: kv 章节"
assert_grep "docs/API_GUIDE.md" "5.4 et_xmodem"          "API_GUIDE: xmodem 章节"
assert_grep "docs/API_GUIDE.md" "8.3 et_shell"           "API_GUIDE: shell 章节"
assert_grep "docs/API_GUIDE.md" "4.4 et_softclock"       "API_GUIDE: softclock 章节"
assert_grep "docs/API_GUIDE.md" "3.2 et_fsm"             "API_GUIDE: fsm 章节"
assert_grep "docs/API_GUIDE.md" "ET_XM_1K"               "API_GUIDE 配置表: ET_XM_1K"
assert_grep "docs/API_GUIDE.md" "6.2 et_bootctl"         "API_GUIDE: bootctl 章节"
assert_grep "docs/API_GUIDE.md" "4.5 et_wdt"             "API_GUIDE: wdt 章节"
assert_grep "docs/API_GUIDE.md" "ET_SHELL_HISTORY_N"     "API_GUIDE 配置表: shell 历史"
assert_grep "Makefile" "storage/et_bootctl.c" "Makefile 含 et_bootctl"
assert_grep "Makefile" "sys/et_wdt.c"         "Makefile 含 et_wdt"
assert_grep "docs/API_GUIDE.md" "flash 契约"             "API_GUIDE: port flash 契约小节"

# ---- Renode smoke 资产自洽 (脚本断言与文档一致) ----
assert_grep "port/stm32f103/README.md" "smoke.sh"        "port README 引用 smoke.sh"
assert_grep "port/stm32f103/renode/smoke.sh" "boot #2"   "smoke.sh 断言: 复位后 boot #2"

# ---- STM32G474 平台 (port/stm32g474) 文档与构建要素 ----
assert_grep "port/stm32g474/README.md" "DPORT_FLASH_SECTOR_SIZE=2048" "port README 含 G4 flash 几何 -D 构建选项"
assert_grep "port/stm32g474/README.md" "RM0440"                      "port README 含 RM0440 规范引用"
assert_grep "port/stm32g474/README.md" "GNU Tools for STM32"         "port README 体积测量环境注记 (工具链版本)"
assert_grep "port/stm32g474/README.md" "15836"                       "port README 体积表 v1.5 行 (text=15836)"
assert_grep "README.md" "port/stm32g474"                             "根 README 目录结构含 stm32g474"
assert_grep "docs/API_GUIDE.md" "STM32G474VET6"                      "API_GUIDE 平台清单含 G474"
assert_grep "port/stm32g474/port_stm32g474.c" "PORT_FLASH_SECTOR_SIZE != 2048" "port 层几何 #error 守卫"

# ---- v1.6 双几何回归 / tickless / 升级链工具 ----
assert_grep "Makefile" "et_tests_g4"                          "Makefile 含双几何变体目标 test-g4"
assert_grep "README.md" "test-g4"                             "README 记载双几何变体命令"
assert_grep ".github/workflows/ci.yml" "cross-stm32g474"      "CI 含 G474 交叉编译 job"
assert_grep "storage/et_kv.h" "ET_KV_VAL_MAX"                 "et_kv 公开容量宏(双几何用例基石)"
assert_grep "docs/API_GUIDE.md" "et_sched_next_due"           "API_GUIDE: sched tickless API"
assert_grep "docs/API_GUIDE.md" "et_stimer_next_due"          "API_GUIDE: stimer tickless API"
assert_grep "docs/API_GUIDE.md" "11.8 tickless"               "API_GUIDE: tickless 配方(WFI/RX 唤醒约束)"
assert_grep "port/port.h" "PORT_TICK_WAIT_FOREVER"            "port.h tickless 哨兵宏"
assert_grep "tools/xmodem_send.py" "XMODEM-CRC"               "xmodem 发送端脚本入库"
assert_grep "移植stm32实机记录.md" "升级链真机走单"            "实机记录含升级链走单章节"

echo "----------------------------------------"
echo "docsync: pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
