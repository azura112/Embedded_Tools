#!/bin/sh
# =====================================================================
# sizecheck.sh —— 体积表逐值比对门 (v2.0 P0-3)
#
# 职责: 本地发布前执行——经 docbuild.sh(单一来源)全量构建 port,
# 取 arm-none-eabi-size 读数, 与 port README 体积表"当前版本行"逐值
# 比对(text/data/bss)。CI 侧不做等值断言(apt 工具链与本机存在
# 布局级差异, v1.9 决议): CI 只上传读数到 artifact/summary 供追溯。
#
# 用法: sh tools/sizecheck.sh          (仓库根; 全绿退出 0)
# 自证: 手改任一当前版本行的任一数字 → 本脚本对应变红。
# =====================================================================
set -u
cd "$(dirname "$0")/.."

FAIL=0
PASS=0

V=$(gcc -E -P -dM et_config.h | awk '/define ET_VERSION_MAJOR/ {maj=$3} /define ET_VERSION_MINOR/ {min=$3} END {print maj"."min}')
if [ -z "$V" ] || [ "${V#*.}" = "" ]; then
    echo "FAIL: 无法从 et_config.h 解析版本分量"
    exit 1
fi
VTAG="v$V"
echo "[sizecheck] 当前版本行: $VTAG"

command -v arm-none-eabi-size >/dev/null 2>&1 || {
    echo "FAIL: 未找到 arm-none-eabi-size (需要 CubeCLT GNU Tools for STM32 在 PATH)"; exit 1; }

if ! sh tools/docbuild.sh >/tmp/sizecheck_build.log 2>&1; then
    echo "FAIL: docbuild 构建失败:"
    tail -5 /tmp/sizecheck_build.log
    exit 1
fi

# check_rows <readme> <elf基名(无扩展)>
#   默认变体 ↔ <基名>.elf; selftest 变体 ↔ <基名>_selftest.elf
#   README 表行分派: 备注含 ET_MODULE_SELFTEST=1 → selftest; 否则 → 默认
check_rows() {
    readme="$1"; base="$2"
    for variant in default selftest; do
        case "$variant" in
        default)  elf="$base.elf"
                  grep -E "^\| $VTAG *\|" "$readme" | grep -v "ET_MODULE_SELFTEST=1" > /tmp/sizecheck_row.$$ || true
                  want="默认" ;;
        selftest) elf="${base}_selftest.elf"
                  grep -E "^\| $VTAG *\|" "$readme" | grep "ET_MODULE_SELFTEST=1" > /tmp/sizecheck_row.$$ || true
                  want="selftest" ;;
        esac

        if [ ! -s /tmp/sizecheck_row.$$ ]; then
            if [ "$variant" = "selftest" ]; then
                echo "ok   $readme [$want] 无该变体表行(port 未建此构建, 跳过)"
            else
                echo "FAIL: $readme 缺 $VTAG [$want] 表行"
                FAIL=$((FAIL + 1))
            fi
            continue
        fi
        if [ ! -f "$elf" ]; then
            echo "FAIL: 缺产物 $elf (docbuild 块未产出?)"; FAIL=$((FAIL + 1)); continue
        fi

        SIZEV=$(arm-none-eabi-size "$elf" | tail -1 | awk '{print $1"/"$2"/"$3}')
        txt=$(echo "$SIZEV" | cut -d/ -f1); dat=$(echo "$SIZEV" | cut -d/ -f2); bss=$(echo "$SIZEV" | cut -d/ -f3)
        # 表行可能多条(同版本重复行取第一条): field2=text field3=data field4=bss
        v_t=$(sed -n 's/^| '"$VTAG"' *| *\([0-9][0-9]*\) *|.*/\1/p' /tmp/sizecheck_row.$$ | head -1)
        v_d=$(sed -n 's/^| '"$VTAG"' *| *[0-9][0-9]* *| *\([0-9][0-9]*\) *|.*/\1/p' /tmp/sizecheck_row.$$ | head -1)
        v_b=$(sed -n 's/^| '"$VTAG"' *| *[0-9][0-9]* *| *[0-9][0-9]* *| *\([0-9][0-9]*\) *|.*/\1/p' /tmp/sizecheck_row.$$ | head -1)
        if [ "$v_t" = "$txt" ] && [ "$v_d" = "$dat" ] && [ "$v_b" = "$bss" ]; then
            echo "ok   $readme $VTAG [$want] 表行 $v_t/$v_d/$v_b = 实测"
            PASS=$((PASS + 1))
        else
            echo "FAIL: $readme $VTAG [$want] 表行 $v_t/$v_d/$v_b ≠ 实测 $txt/$dat/$bss"
            echo "     → 数字回刷纪律(checklist #7): 同提交更新表行"
            FAIL=$((FAIL + 1))
        fi
    done
    rm -f /tmp/sizecheck_row.$$
}

check_rows port/stm32f103/README.md build/stm32f103_demo
check_rows port/stm32g474/README.md build/stm32g474_demo

echo "----------------------------------------"
echo "sizecheck: pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
