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

# 反向断言: 模式不得出现 (占位符清除/滞后值回刷类治理)
assert_no_grep() {
    file="$1"; pat="$2"; desc="$3"
    if [ ! -f "$file" ]; then
        echo "FAIL [文件缺失] $desc ($file)"
        FAIL=$((FAIL + 1))
        return
    fi
    if grep -Eq -- "$pat" "$file"; then
        echo "FAIL $desc  (文件 $file 不应命中: $pat)"
        FAIL=$((FAIL + 1))
    else
        echo "ok   $desc"
        PASS=$((PASS + 1))
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
assert_grep "port/stm32g474/README.md" "不排期"               "Renode G4 政策关闭(v1.7)"
assert_grep "et_config.h" "ET_MODULE_SELFTEST"                "et_config 含 selftest 模块开关(默认裁剪)"
assert_grep "Makefile" "debug/et_selftest.c"                  "Makefile 含 et_selftest 源"
assert_grep "Makefile" "bench"                                "Makefile 含 bench 目标"
assert_grep "README.md" "et_selftest"                         "README 特性表: selftest"
assert_grep "docs/API_GUIDE.md" "8.4 et_selftest"             "API_GUIDE: selftest 章节"
assert_grep "docs/bench.md" "环境注记"                        "bench 文档含环境注记条款"
assert_grep "docs/bench.md" "v1.7.0"                          "bench 文档含基线版本行"
# (v1.9 治理) CHANGELOG 版本链动态全覆盖 —— 写死单版本断言曾漏过 v1.8 缺条目
if [ -n "$V" ]; then
    MAJ=$(echo "$V" | cut -d. -f1); MIN=$(echo "$V" | cut -d. -f2)
    ci=0
    while [ "$ci" -le "$MIN" ]; do
        if grep -Eq "^## \[$MAJ\.$ci\.0\]" CHANGELOG.md; then
            echo "ok   CHANGELOG 含 $MAJ.$ci.0 条目"; PASS=$((PASS + 1))
        else
            echo "FAIL CHANGELOG 缺 $MAJ.$ci.0 条目(版本链断档)"; FAIL=$((FAIL + 1))
        fi
        ci=$((ci + 1))
    done
fi

# ---- v1.8 et_map / et_xmodem_tx ----
assert_grep "core/et_map.h" "ET_MAP_KEY_TOMB"                 "et_map 保留键语义(头文件)"
assert_grep "core/et_map.h" "负载因子"                        "et_map 容量指引(头文件)"
assert_grep "protocol/et_xmodem_tx.h" "et_xmodem_crc16"       "et_xmodem_tx 共享 CRC 助手"
assert_grep "Makefile" "core/et_map.c"                        "Makefile 含 et_map"
assert_grep "Makefile" "protocol/et_xmodem_tx.c"              "Makefile 含 et_xmodem_tx"
assert_grep "README.md" "et_map"                              "README 特性表: map"
assert_grep "README.md" "et_xmodem_tx"                        "README 特性表: xmodem_tx"
assert_grep "docs/API_GUIDE.md" "2.5 et_map"                  "API_GUIDE: map 章节"
assert_grep "docs/API_GUIDE.md" "5.5 et_xmodem_tx"            "API_GUIDE: xmodem_tx 章节"
assert_grep ".github/workflows/ci.yml" "DET_MODULE_SELFTEST=1" "CI host 构建启用 selftest 复跑"

# ---- v1.9 et_smap / docbuild / 走单收口 ----
assert_grep "core/et_smap.h"      "ET_SMAP_KEY_MAX"           "et_smap 键长上限(头文件)"
assert_grep "core/et_smap.h"      "池满"                      "et_smap 池满拒绝语义(头文件)"
assert_grep "core/et_smap.c"      "2166136261"                "et_smap FNV-1a 实现"
assert_grep "et_config.h"         "ET_MODULE_SMAP"            "et_config 含 SMAP 开关"
assert_grep "Makefile"            "core/et_smap.c"            "Makefile 含 et_smap"
assert_grep "Makefile"            "test/test_smap.c"          "Makefile 含 test_smap"
assert_grep "test/test_main.c"    "test_smap_cases"           "test_main 注册 smap 套件"
assert_grep "README.md"           "et_smap"                   "README 特性表: smap"
assert_grep "docs/API_GUIDE.md"   "2.6 et_smap"               "API_GUIDE: smap 章节"
assert_grep "docs/API_GUIDE.md"   "11.9 字符串键配置表"        "API_GUIDE: smap 配方(P3-3)"
assert_grep "docs/API_GUIDE.md"   "key.1 偏移"                "API_GUIDE: et_map 键 0 FAQ(决议)"
assert_grep "docs/API_GUIDE.md"   "ET_SMAP_KEY_MAX"           "API_GUIDE 配置表: SMAP 键长"
assert_grep "docs/bench.md"       "smap str get"              "bench 含 smap 查找行(P3-2)"
assert_grep "protocol/et_crc.c"   "s_crc32_tbl"               "et_crc CRC32 查表路径(P3-1)"
assert_grep "tools/docbuild.sh"   "docbuild"                  "docbuild 脚本存在(P1-3)"
assert_grep "port/stm32f103/README.md" "```docbuild"          "f103 README docbuild 定界"
assert_grep "port/stm32g474/README.md"  "```docbuild"         "g474 README docbuild 定界"
assert_grep ".github/workflows/ci.yml"  "docbuild"            "CI 含 docbuild job"
assert_grep "README.md"           "数字回刷纪律"              "README checklist 第 7 条(P1-4)"
assert_grep "tools/pack_image.py" "ETBI"                      "ETBI 打包工具存在(走单3配套)"
assert_grep "port/stm32f103/README.md" "v1.9"                 "体积表含 v1.9 行(f103)"
assert_grep "port/stm32g474/README.md"  "18088"               "体积表 v1.9 行(g474)"
assert_grep "port/stm32f103/README.md"  "25352"               "体积表 v1.9 行(f103, selftest)"
assert_grep "移植stm32实机记录.md" "SELFTEST: 20/20"          "实机记录含 v2.2 selftest 20 套件板上记录(P1, 历史节)"
assert_grep "移植stm32实机记录.md" "SELFTEST: 22/22"          "实机记录含 v2.7 selftest 22 套件板上记录(P1-4/HC-9)"
assert_grep "移植stm32实机记录.md" "reset cause: IWDG"        "实机记录含 IWDG 真超时证据(P0-1 走单4)"
assert_no_grep "移植stm32实机记录.md" "待上板执行"            "实机记录走单占位符已全部回填(P0-1)"

# ---- v2.0 P0 单一来源 / sizecheck / 数字回刷 ----
assert_grep "tools/sizecheck.sh"  "arm-none-eabi-size"         "sizecheck 门存在(P0-3)"
assert_grep "tools/sizecheck.sh"  "docbuild"                   "sizecheck 走 docbuild 单一来源"
assert_no_grep ".github/workflows/ci.yml"     "-o build/stm32f103_demo.elf"  "CI 无内联 F103 构建清单(P0-2 单一来源)"
assert_no_grep ".github/workflows/release.yml" "-o build/stm32f103_demo.elf" "Release 无内联构建清单(P0-2)"
assert_grep ".github/workflows/ci.yml"        "tools/docbuild.sh"   "CI 转调 docbuild(交叉/仿真)"
assert_grep ".github/workflows/release.yml"   "tools/docbuild.sh"   "Release 转调 docbuild"
assert_grep "Makefile"                        "coverage-build"      "Makefile coverage 单源目标(P0-2)"
assert_grep ".github/workflows/ci.yml"        "make coverage-build" "CI coverage 转调 Makefile"
assert_grep "port/stm32f103/renode/smoke.sh"  "_selftest.elf"       "smoke 消费 selftest 变体产物"
assert_grep "port/stm32f103/README.md"        "17836"               "体积表 v2.0 默认行(终值回刷)"
assert_grep "port/stm32f103/README.md"        "26304"               "体积表 v2.0 selftest 行(终值回刷)"
assert_grep "port/stm32g474/README.md"        "18208"               "体积表 v2.0 行(g474, 终值回刷)"
assert_grep "README.md"                       "sizecheck"           "checklist/README 引用 sizecheck 门"
assert_grep "port/_template/port_template.c" "port_flash_erase_sector"  "移植模板含 flash 契约(四件套)"
assert_grep "port/_template/port_template.c" "port_wdt_enable"          "移植模板含 wdt 三件套"
assert_grep "port/_template/port_template.c" "port_critical_enter"      "移植模板含临界区"
assert_grep "port/_template/port_template.c" "port_tick_get_ms"         "移植模板含时基"
assert_grep "port/_template/port_template.c" "#error"                   "移植模板含几何守卫"
assert_grep "port/_template/README.md"       "双几何回归硬性项"         "移植 checklist 含双几何"
assert_grep ".github/workflows/release.yml"  "body_path: release_body.md" "Release 正文注入 CHANGELOG 节(P3-1)"

# ---- v2.0 P1 冻结契约三件套 + 清单一致性机制 ----
if sh tools/apidump.sh --check >/tmp/docsync_apidump.log 2>&1; then
    echo "ok   API 清单与头文件一致 (apidump --check)"; PASS=$((PASS + 1))
else
    echo "FAIL API 清单漂移: 公开面变更未重新生成 docs/API_INVENTORY.md"; sed 's/^/     /' /tmp/docsync_apidump.log | head -8
    FAIL=$((FAIL + 1))
fi
assert_grep "docs/API_STABILITY.md"  "冻结声明"                  "冻结声明存在(P1-2)"
assert_grep "docs/API_STABILITY.md"  "v3-candidates"             "审计处置含 v3 候选去向(P1-3)"
assert_grep "docs/API_STABILITY.md"  "一致性审计"                "审计表在案(P1-3)"
assert_grep "docs/v3-candidates.md"  "v2.0 决议"                 "候选关闭决议落档(P2 零堆积)"
assert_grep "et_config.h"            "ET_DEPRECATED"             "ET_DEPRECATED 宏存在(P1-2)"
assert_grep "README.md"              "API_STABILITY"             "README 链接冻结声明(P1-4)"
assert_grep "docs/API_GUIDE.md"      "API_STABILITY"             "API_GUIDE 链接冻结声明(P1-4)"
assert_grep "CHANGELOG.md"           "API freeze"                "CHANGELOG 标注 v2.0 API freeze(P1-4)"

# ---- v2.1 P0-1 冻结基线 + apidump --diff 机检 ----
assert_grep "tools/apidump.sh"           "\\-\\-snapshot"            "apidump 基线归档入口(P0-1)"
assert_grep "tools/apidump.sh"           "\\-\\-diff"                "apidump 纯增比对入口(P0-1)"
assert_grep "tools/apidump.sh"           "ET_VERSION"                "apidump --diff 版本宏忽略规则(P0-1)"
assert_grep "docs/API_INVENTORY_v2.0.md" "自动生成"                  "v2.0 冻结基线已归档(P0-1)"
assert_grep "docs/API_STABILITY.md"      "apidump --diff"            "API_STABILITY 记录 --diff 规则(附则)"
assert_grep "docs/API_STABILITY.md"      "字段追加"                  "API_STABILITY 结构体字段登记规则"
assert_grep "README.md"                  "apidump"                   "README 引用 apidump 冻结机检"

# ---- v2.1 P1/P2/P3 新模块 (et_pid / et_stats / et_bytes) ----
assert_grep "et_config.h"         "ET_MODULE_PID"       "et_config 含 PID 开关"
assert_grep "et_config.h"         "ET_MODULE_STATS"     "et_config 含 STATS 开关"
assert_grep "et_config.h"         "ET_MODULE_BYTES"     "et_config 含 BYTES 开关"
assert_grep "Makefile"            "algorithm/et_pid.c"  "Makefile 含 et_pid 源"
assert_grep "Makefile"            "algorithm/et_stats.c" "Makefile 含 et_stats 源"
assert_grep "Makefile"            "protocol/et_bytes.c" "Makefile 含 et_bytes 源"
assert_grep "Makefile"            "test/test_pid.c"     "Makefile 含 test_pid"
assert_grep "Makefile"            "test/test_stats.c"   "Makefile 含 test_stats"
assert_grep "Makefile"            "test/test_bytes.c"   "Makefile 含 test_bytes"
assert_grep "test/test_main.c"    "test_pid_cases"      "test_main 注册 pid 套件"
assert_grep "test/test_main.c"    "test_stats_cases"    "test_main 注册 stats 套件"
assert_grep "test/test_main.c"    "test_bytes_cases"    "test_main 注册 bytes 套件"
assert_grep "algorithm/et_pid.h"  "溢出语义"            "et_pid 溢出语义头注(P1)"
assert_grep "algorithm/et_pid.h"  "积分限幅"            "et_pid 抗饱和决议头注(P1)"
assert_grep "algorithm/et_pid.h"  "d_on_measure"        "et_pid 微分对象可选(P1)"
assert_grep "algorithm/et_stats.h" "Welford"            "et_stats Welford 算法声明(P2)"
assert_grep "algorithm/et_stats.h" "var_q10"            "et_stats Q10 方差标度声明(P2)"
assert_grep "protocol/et_bytes.h" "边界检查"            "et_bytes 边界检查语义(P3)"
assert_grep "README.md"           "et_pid"              "README 特性表: pid"
assert_grep "README.md"           "et_stats"            "README 特性表: stats"
assert_grep "README.md"           "et_bytes"            "README 特性表: bytes"
assert_grep "docs/API_GUIDE.md"   "3.3 et_pid"          "API_GUIDE: pid 章节"
assert_grep "docs/API_GUIDE.md"   "3.4 et_stats"        "API_GUIDE: stats 章节"
assert_grep "docs/API_GUIDE.md"   "5.6 et_bytes"        "API_GUIDE: bytes 章节"
assert_grep "docs/API_GUIDE.md"   "11.10 定点闭环整定配方" "API_GUIDE: 整定配方(P1)"
assert_grep "docs/bench.md"       "pid step"            "bench 含 pid 行(P3-3)"
assert_grep "docs/bench.md"       "stats push"          "bench 含 stats 行(P3-3)"
assert_grep "docs/bench.md"       "v2.1.0"              "bench 文档含 v2.1 版本行"
assert_grep "docs/getting-started.md" "从零到板上"       "getting-started 端到端教程(P3-2)"
assert_grep "docs/getting-started.md" "port/_template"  "getting-started 链接移植模板"
assert_grep "docs/getting-started.md" "497"             "getting-started 用例数与实测一致(v2.7 回刷)"
assert_grep "README.md"           "getting-started"     "README 链接上手教程(P3-2)"
assert_grep "README.md"           "497"                 "README 用例数终值回刷(v2.7)"

# ---- v1.8 覆盖率行治理: 每份交付文档复现表必须含覆盖率行 ----
assert_grep "README.md" "行覆盖"                                 "README 含覆盖率行(测试与质量门)"
for f in v[0-9]*开发交付*.md; do  # v2.0 起 glob 兼容双位数版本(原 v1.* 会静默漏掉 v2 交付文档)
    if ! grep -q "覆盖率" "$f"; then
        echo "FAIL 交付复现表缺覆盖率行: $f"
        FAIL=$((FAIL + 1))
    else
        echo "ok   交付文档覆盖率行: $f"
        PASS=$((PASS + 1))
    fi
done

# ---- v2.2 P0 基线滚动 + P1 selftest 20 套件 + P3 medfilt + P4 sched stats ----
assert_grep "tools/apidump.sh"           "API_INVENTORY_v2.6.md"     "apidump 默认基线滚动至 v2.6(v2.8 P0-2 滚动规则, 清偿 CO-2(v2.7) 滞留)"
assert_grep "docs/API_INVENTORY_v2.1.md" "自动生成"                  "v2.1 冻结基线已归档(只读保留)"
assert_grep "docs/API_INVENTORY_v2.2.md" "自动生成"                  "v2.2 冻结基线已归档(只读保留)"
assert_grep "docs/API_INVENTORY_v2.3.md" "自动生成"                  "v2.3 冻结基线已归档(P0-1 滚动)"
assert_grep "docs/API_STABILITY.md"      "字段追加登记区"             "API_STABILITY 结构体字段登记区(P4 附则实战)"
assert_grep "et_config.h"         "ET_MODULE_MEDFILT"   "et_config 含 MEDFILT 开关(P3)"
assert_grep "et_config.h"         "ET_MEDFILT_WIN_MAX"  "et_config 含 medfilt 窗上限(P3)"
assert_grep "Makefile"            "algorithm/et_medfilt.c" "Makefile 含 et_medfilt 源(P3)"
assert_grep "Makefile"            "test/test_medfilt.c" "Makefile 含 test_medfilt(P3)"
assert_grep "test/test_main.c"    "test_medfilt_cases"  "test_main 注册 medfilt 套件(P3)"
assert_grep "README.md"           "et_medfilt"          "README 特性表: medfilt(P3)"
assert_grep "docs/API_GUIDE.md"   "3.5 et_medfilt"      "API_GUIDE: medfilt 章节(P3)"
assert_grep "docs/API_GUIDE.md"   "ET_MEDFILT_WIN_MAX"  "API_GUIDE 配置表: medfilt 窗上限(P3)"
assert_grep "algorithm/et_medfilt.h" "奇数"             "et_medfilt 奇数窗决议(头注,P3)"
assert_grep "sys/et_sched.h"      "et_sched_task_stats" "et_sched 任务耗时统计 API(P4)"
assert_grep "test/test_sched.c"   "sc_stats_measures_duration" "sched 耗时计量用例(P4)"
assert_grep "debug/et_selftest.c" "st_pid"              "selftest 含 pid 套件(P1)"
assert_grep "debug/et_selftest.c" "st_stats"            "selftest 含 stats 套件(P1)"
assert_grep "debug/et_selftest.c" "st_bytes"            "selftest 含 bytes 套件(P1)"
assert_grep "port/stm32f103/renode/smoke.sh" "SELFTEST: 22/22" "smoke 断言同步 22 套件(v2.7 P1-3)"
assert_grep "protocol/et_crc.c"   "s_modbus_tbl"        "CRC16-MODBUS 查表路径(P5-3)"
assert_grep "docs/bench.md"       "medfilt push"        "bench 含 medfilt 行(P5-2)"
assert_grep "docs/bench.md"       "v2.2.0"              "bench 文档含 v2.2 版本行"
assert_grep "docs/API_GUIDE.md"   "11.11 kv"            "API_GUIDE: kv 备份恢复配方(P5-1)"

# ---- v2.3 P0 基线滚动 + P1 et_hist + P2 配方可执行化 + P3 architecture ----
assert_grep "et_config.h"         "ET_MODULE_HIST"      "et_config 含 HIST 开关(P1)"
assert_grep "et_config.h"         "ET_HIST_BIN_MAX"     "et_config 含 hist 桶上限(P1)"
assert_grep "Makefile"            "algorithm/et_hist.c" "Makefile 含 et_hist 源(P1)"
assert_grep "Makefile"            "test/test_hist.c"    "Makefile 含 test_hist(P1)"
assert_grep "test/test_main.c"    "test_hist_cases"     "test_main 注册 hist 套件(P1)"
assert_grep "README.md"           "et_hist"             "README 特性表: hist(P1)"
assert_grep "docs/API_GUIDE.md"   "3.6 et_hist"         "API_GUIDE: hist 章节(P1)"
assert_grep "docs/API_GUIDE.md"   "11.12 任务耗时分布诊断" "API_GUIDE: taskhist 配方(P1)"
assert_grep "algorithm/et_hist.h" "闭区间"              "et_hist 闭区间语义(头注,P1)"
assert_grep "algorithm/et_hist.h" "粗估"                "et_hist 百分位粗估语义(头注,P1)"
assert_grep "Makefile"            "^ex:"                "Makefile 含 make ex 目标(P2)"
assert_grep "examples/ex_pid_loop.c"   "11.10"          "示例1 载体标注 11.10(P2)"
assert_grep "examples/ex_kv_backup.c"  "11.11"          "示例2 载体标注 11.11(P2)"
assert_grep "examples/ex_upgrade_flow.c" "bootctl"      "示例3 升级流程载体(P2)"
assert_grep "examples/ex_pid_loop.c"   "PASS"           "示例1 自检输出(P2)"
assert_grep "docs/API_GUIDE.md"   "ex_pid_loop"         "API_GUIDE 互链: 闭环载体(P2)"
assert_grep "docs/API_GUIDE.md"   "ex_kv_backup"        "API_GUIDE 互链: kv 备份载体(P2)"
assert_grep "docs/API_GUIDE.md"   "ex_upgrade_flow"     "API_GUIDE 互链: 升级载体(P2)"
assert_grep ".github/workflows/ci.yml" "make ex"        "CI 常设 make ex(P2)"
assert_grep "docs/architecture.md" "分层"                "architecture.md 在档(P3)"
assert_grep "docs/architecture.md" "模块选型"             "architecture.md 选型导航(P3)"
assert_grep "docs/v3-candidates.md" "biquad"             "v2.4 候选: 定点 biquad 评估(P4-2)"
assert_grep "docs/bench.md"       "hist push"           "bench 含 hist 行(P4-1)"
assert_grep "docs/bench.md"       "v2.3.0"              "bench 文档含 v2.3 版本行"

# ---- v2.4 P0 基线滚动/biquad 判定 + P1 et_modbus + P2 板侧回归 ----
assert_grep "et_config.h"         "ET_MODULE_MODBUS"    "et_config 含 MODBUS 开关(P1)"
assert_grep "Makefile"            "protocol/et_modbus.c" "Makefile 含 et_modbus 源(P1)"
assert_grep "Makefile"            "test/test_modbus.c"  "Makefile 含 test_modbus(P1)"
assert_grep "Makefile"            "ex_modbus_slave"     "Makefile ex 含 modbus 示例(P1)"
assert_grep "test/test_main.c"    "test_modbus_cases"   "test_main 注册 modbus 套件(P1)"
assert_grep "README.md"           "et_modbus"           "README 特性表: modbus(P1)"
assert_grep "docs/API_GUIDE.md"   "5.7 et_modbus"       "API_GUIDE: modbus 章节(P1)"
assert_grep "docs/API_GUIDE.md"   "11.13 kv 参数暴露为保持寄存器" "API_GUIDE: kv 直通配方(P1)"
assert_grep "docs/API_GUIDE.md"   "ET_MODBUS_RD_QTY_MAX" "API_GUIDE modbus 数量边界(P1)"
assert_grep "protocol/et_modbus.h" "低字节在前"          "et_modbus CRC 线上序(头注,P1)"
assert_grep "protocol/et_modbus.h" "广播"                "et_modbus 广播语义(头注,P1)"
assert_grep "examples/ex_modbus_slave.c" "PASS"          "modbus 示例自检输出(P1)"
assert_grep "tools/modbus_master.py" "selftest"          "主站工具回环自测入口(P1)"
assert_grep "tools/modbus_master.py" "0x4B37"            "主站工具 CRC 标准向量自检(P1)"
assert_grep "docs/architecture.md" "et_modbus"           "architecture 选型表含 modbus(P1)"
assert_grep "docs/bench.md"       "modbus"               "bench 含 modbus 行(P3-1)"
assert_grep "docs/bench.md"       "v2.4.0"               "bench 文档含 v2.4 版本行"
assert_grep "docs/v3-candidates.md" "维持排队"            "biquad 判定决议落档(P0-2)"
assert_grep "移植stm32实机记录.md" "modbus"               "实机记录含 Modbus 走单章节(P2)"

# ---- v2.5 P0 基线滚动/评审记录载体 + P1 et_log 加固 + P2 et_modbus_master ----
assert_grep "tools/apidump.sh"           "API_INVENTORY_v2.6.md"     "apidump 默认基线滚动至 v2.6(v2.8 P0-2 滚动规则)"
assert_grep "docs/API_INVENTORY_v2.6.md" "自动生成"                  "v2.6 冻结基线已归档(v2.8 P0-1 滚动, 清偿 CO-2(v2.7))"
assert_grep "docs/v3-candidates.md"      "v2.5 复评记录"             "候选池 v2.5 复评记录落档(P0-2)"
assert_grep "docs/v3-candidates.md"      "单事务"                    "主站边界: 单事务不含调度器(P0-2/HC-4)"
assert_grep "docs/v3-candidates.md"      "CO-5"                      "32 位组合封装拒绝理由落档(P0-2)"
assert_grep "docs/评审记录模板.md"        "CO 编号规则"               "评审记录模板: CO 编号规则(P0-4)"
assert_grep "docs/评审记录模板.md"        "触发条件"                  "评审记录模板: 处置字段含触发条件(P0-4)"
assert_grep "README.md"                  "每版交付后产出评审记录"     "README checklist 含评审记录行(P0-4)"
# (自指) 当前版本交付文档须引用评审记录或显式声明其缺失 —— 关闭"评审记录持续缺失"风险
if [ -n "$V" ]; then
    VMM2=$(echo "$V" | cut -d. -f1,2)
    doc2=$(ls v${VMM2}开发交付*.md 2>/dev/null | head -1)
    if [ -z "$doc2" ]; then
        echo "FAIL [评审记录] 缺当前版本交付文档 v${VMM2}开发交付*.md"; FAIL=$((FAIL + 1))
    elif grep -Eq "评审记录" "$doc2"; then
        echo "ok   交付文档引用评审记录或声明缺失(P0-4)"; PASS=$((PASS + 1))
    else
        echo "FAIL [评审记录] $doc2 既未引用评审记录也未声明其缺失(P0-4)"; FAIL=$((FAIL + 1))
    fi
fi
assert_grep "debug/et_log.h"      "ET_LOG_FIELD_MAX"       "et_log 域宽上限常量(P1-1)"
assert_grep "debug/et_log.h"      "可见占位"                "et_log 不支持规格的可见占位规约(P1-2/HC-3)"
assert_grep "Makefile"            "test/test_modbus_master.c" "Makefile 含 test_modbus_master(P1-7)"
assert_grep "Makefile"            "protocol/et_modbus_master.c" "Makefile 含 et_modbus_master 源(P1-6)"
assert_grep "Makefile"            "ex_modbus_master"       "Makefile ex 含主站示例(P1-8)"
assert_grep "test/test_main.c"    "test_modbus_master_cases" "test_main 注册 modbus_master 套件(P1-7)"
assert_grep "README.md"           "et_modbus_master"       "README 特性表: modbus_master(P1-11)"
assert_grep "docs/API_GUIDE.md"   "5.8 et_modbus_master"   "API_GUIDE: 主站章节(P1-11)"
assert_grep "docs/API_GUIDE.md"   "11.14"                  "API_GUIDE: 主站轮询协作配方(P1-11)"
assert_grep "docs/API_GUIDE.md"   "应答有两条产生路径"      "API_GUIDE 5.7 范式修正(CO-8/P1-10)"
assert_grep "protocol/et_modbus_master.h" "单事务"          "主站单事务语义(头注, HC-4)"
assert_grep "protocol/et_modbus_master.h" "et_modbus.h"     "主站复用从站协议常量(HC-5)"
assert_grep "examples/ex_modbus_master.c" "PASS"            "主站示例自检输出(P1-8)"
assert_grep "examples/ex_modbus_slave.c"  "tick"            "从站示例含 tick 路径应答断言(P1-10)"
assert_grep "tools/modbus_master.py" "slave-drop"           "从站仿真器丢包注入(P1-9)"
assert_grep "tools/modbus_master.py" "slave-exc"            "从站仿真器异常注入(P1-9)"
assert_grep "docs/architecture.md" "et_modbus_master"       "architecture 选型表含主站(P1-11)"
assert_grep "docs/bench.md"       "modbus_master"           "bench 含主站行(P3-1)"
assert_grep "docs/bench.md"       "v2.5.0"                  "bench 文档含 v2.5 版本行"
assert_grep "docs/API_GUIDE.md"   "32 位参数"               "API_GUIDE 11.13 补 32 位组合说明(P3-3)"
assert_grep "移植stm32实机记录.md" "v2.5.0"                 "实机记录含 v2.5 板侧章节(P2)"

# ---- v2.6 P0-2 跨文档计数一致性 (CO-3/CO-4 漏刷的根因根治, HC-4) ----
# 单一来源 = README(交付时回刷的唯一权威); architecture 机制门行与 API_GUIDE 的
# 对应数字必须**与实测/README 一致**, 而非各自写死。v2.5 的 examples 数(architecture
# 仍写"四例")与用例数(API_GUIDE 仍写 398) 无门守护即漏刷 —— 本断言把它们变成硬门。
# 门自证可红: 故意改任一处数字 → 本节 FAIL(证据记交付文档 §7 机制自证表)。
cn_num() {                              # 中文数字 → 阿拉伯(1~10: examples 数够用)
    case "$1" in
        一) echo 1 ;; 二) echo 2 ;; 三) echo 3 ;; 四) echo 4 ;; 五) echo 5 ;;
        六) echo 6 ;; 七) echo 7 ;; 八) echo 8 ;; 九) echo 9 ;; 十) echo 10 ;;
        *)  echo 0 ;;
    esac
}

rd_cases_a=$(sed -n 's/.*ALL PASS（\([0-9][0-9]*\) 例.*/\1/p' README.md | head -1)
rd_cases_b=$(sed -n 's/.*迷你框架 + \([0-9][0-9]*\) 个单元用例.*/\1/p' README.md | head -1)
ar_cases=$(sed -n 's/.*host 单测 *(\([0-9][0-9]*\) 用例.*/\1/p' docs/architecture.md | head -1)
ag_cases_a=$(sed -n 's/.*PC 单测(\([0-9][0-9]*\)).*/\1/p' docs/API_GUIDE.md | head -1)
ag_cases_b=$(sed -n 's/.*冒烟非对等 host \([0-9][0-9]*\) 用例.*/\1/p' docs/API_GUIDE.md | head -1)
if [ -z "${rd_cases_a:-}" ] || [ -z "${rd_cases_b:-}" ] || [ -z "${ar_cases:-}" ] \
   || [ -z "${ag_cases_a:-}" ] || [ -z "${ag_cases_b:-}" ]; then
    echo "FAIL 跨文档用例数断言: 解析失败(模式未命中) README=$rd_cases_a/$rd_cases_b arch=$ar_cases guide=$ag_cases_a/$ag_cases_b"
    FAIL=$((FAIL + 1))
elif [ "$rd_cases_a" = "$rd_cases_b" ] && [ "$rd_cases_a" = "$ar_cases" ] \
     && [ "$rd_cases_a" = "$ag_cases_a" ] && [ "$rd_cases_a" = "$ag_cases_b" ]; then
    echo "ok   跨文档用例数一致 ($rd_cases_a): README×2 / architecture / API_GUIDE×2"
    PASS=$((PASS + 1))
else
    echo "FAIL 跨文档用例数不一致: README=$rd_cases_a/$rd_cases_b arch=$ar_cases guide=$ag_cases_a/$ag_cases_b"
    FAIL=$((FAIL + 1))
fi

ex_files=$(ls examples/ex_*.c 2>/dev/null | grep -c .)
ar_ex=$(sed -n 's/.*make ex: \(.\)例自检式示例.*/\1/p' docs/architecture.md | head -1)
rd_ex=$(sed -n 's/.*扩至\(.\)例.*/\1/p' README.md | head -1)
ar_ex_n=$(cn_num "${ar_ex:-}")
rd_ex_n=$(cn_num "${rd_ex:-}")
if [ "$ex_files" -gt 0 ] && [ "$ex_files" = "$ar_ex_n" ] && [ "$ex_files" = "$rd_ex_n" ]; then
    echo "ok   跨文档 examples 数一致 ($ex_files): examples/ex_*.c 实测 / architecture / README"
    PASS=$((PASS + 1))
else
    echo "FAIL 跨文档 examples 数不一致: 实测=$ex_files arch=$ar_ex→$ar_ex_n README=$rd_ex→$rd_ex_n"
    FAIL=$((FAIL + 1))
fi

# ---- v2.6-r2 G0′-4 同族漏刷三处装门 (CO-6/CO-7/CO-11(v2.6)) ----
# 背景: v2.6 的"装门"只盖住了被点名的三处, 同族的另外三处(README 冻结基线名 /
# port README 板侧叙述 / 计划模板项数)无门 → 漏刷第四例。本节把它们也变成硬门。
# 门自证可红: 改 README 的基线文件名 / 在 port README 写回"未同步" → 本节 FAIL。
rd_base=$(sed -n 's/.*当前 `\(docs\/API_INVENTORY_v[0-9][0-9.]*\.md\)`.*/\1/p' README.md | head -1)
ad_base=$(sed -n 's/.*BASE="${2:-\([^}]*\)}".*/\1/p' tools/apidump.sh | head -1)
if [ -n "${rd_base:-}" ] && [ -n "${ad_base:-}" ] && [ "$rd_base" = "$ad_base" ]; then
    echo "ok   README 冻结基线名与 apidump 默认基线一致 ($rd_base)"
    PASS=$((PASS + 1))
else
    echo "FAIL README 冻结基线名 [${rd_base:-未解析}] ≠ apidump 默认基线 [${ad_base:-未解析}] (CO-6(v2.6) 同族漏刷)"
    FAIL=$((FAIL + 1))
fi
assert_no_grep "port/stm32g474/README.md" "未同步" "g474 README 无'未同步'残留(板侧叙述与实际一致, CO-7(v2.6))"
assert_grep    "docs/开发计划模板.md"     "证据落点章节号" "计划模板含第 5 项(证据落点章节号 + 占位节清理, CO-11(v2.6))"
assert_grep    "README.md"                "核验清单五项"   "README checklist 第 10 条与模板项数一致(五项)"

# ---- v2.1 P0-2 自指断言: 交付文档声明的 docsync 计数 = 本轮实测(含本断言) ----
# 约定(README checklist #8): 当前版本交付文档须有一行 **行首**(允许 markdown 引用/表格
#   前缀 > 或 |)以 "本版 docsync" 开头, 形如 "> 本版 docsync：**186/186**";
#   只校验该行(历史引用如 "v2.0 docsync 142" 不在范围, 计划 §7 风险对策)。
if [ -n "$V" ]; then
    VMM=$(echo "$V" | cut -d. -f1,2)          # 交付文档名按 主.次 (如 v2.1开发交付__...)
    doc=$(ls v${VMM}开发交付*.md 2>/dev/null | head -1)
    expect=$((PASS + 1))            # 本断言本身计入总数 → 文档声明值须等于 expect
    if [ -z "$doc" ]; then
        echo "FAIL [自指] 缺当前版本交付文档 v${VMM}开发交付*.md"
        FAIL=$((FAIL + 1))
    else
        line=$(grep -E '^[[:space:]]*[>|]?[[:space:]]*本版 docsync' "$doc" 2>/dev/null | head -1)
        # 只认声明本身的 "docsync<分隔>N/N" 形态: 分隔符限定为空格/冒号/星号,
        # 避免把同一行里的 "docsync.sh ... §7" 之类文字误当计数(自证时抓到)
        claims=$(printf '%s' "$line" \
                 | grep -oE 'docsync[[:space:]:：*]*[0-9]+/[0-9]+' | grep -oE '[0-9]+' | sort -u)
        if [ -z "$claims" ]; then
            echo "FAIL [自指] $doc 未声明行首 '本版 docsync：N/N' (自指断言无法校验)"
            FAIL=$((FAIL + 1))
        else
            bad=0
            for c in $claims; do
                [ "$c" = "$expect" ] || bad=1
            done
            # v2.6 P0-2 (HC-4): architecture 机制门行的 docsync 计数须与同一实测值一致
            ar_ds=$(sed -n 's/.*docsync \([0-9][0-9]*\) 断言.*/\1/p' docs/architecture.md | head -1)
            if [ -z "${ar_ds:-}" ] || [ "$ar_ds" != "$expect" ]; then
                bad=1
                echo "FAIL [自指] docs/architecture.md 机制门行 docsync 计数 [${ar_ds:-未解析}] ≠ 实测 $expect"
            fi
            if [ "$bad" -eq 0 ]; then
                echo "ok   自指断言: 交付文档与 architecture 机制门行的 docsync 计数 = 实测 $expect"
                PASS=$((PASS + 1))
            else
                echo "FAIL [自指] $doc 声明 docsync [$(echo $claims | tr '\n' ' ')] ≠ 实测 $expect"
                FAIL=$((FAIL + 1))
            fi
        fi
    fi
fi

echo "----------------------------------------"
echo "docsync: pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
