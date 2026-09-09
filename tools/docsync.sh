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
assert_grep "移植stm32实机记录.md" "SELFTEST: 17/17"          "实机记录含库化 selftest 板上记录(P0-2)"
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
assert_grep "port/stm32f103/README.md"        "17712"               "体积表 v2.0 默认行(实测回刷)"
assert_grep "port/stm32f103/README.md"        "26180"               "体积表 v2.0 selftest 行"
assert_grep "port/stm32g474/README.md"        "18084"               "体积表 v2.0 行(g474)"
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

echo "----------------------------------------"
echo "docsync: pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
