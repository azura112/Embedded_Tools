#!/bin/sh
# =====================================================================
# apidump.sh —— 公开 API 清单生成器 (v2.0 P1-1)
#
# 从全部公开头文件提取: 函数签名(声明)/typedef/公开 ET_ 与 PORT_ 宏,
# 生成 docs/API_INVENTORY.md —— API 冻结(docs/API_STABILITY.md)的
# 机读事实清单: **新增公开面未重新生成清单 → docsync 即红**
# (防"冻结承诺与实际脱节", 沿 docsync 治理同构)。
#
# 用法:
#   sh tools/apidump.sh                        # 重新生成 docs/API_INVENTORY.md
#   sh tools/apidump.sh --check                # 与已入库清单比对, 漂移退出非零
#   sh tools/apidump.sh --snapshot <清单文件>  # 固化冻结基线 (v2.1 P0-1)
#   sh tools/apidump.sh --diff [基线清单]      # 与基线比对: 纯新增=绿 (默认
#                                              #   基线 docs/API_INVENTORY_v2.0.md)
# 提取口径(见计划 §7 风险对策): 公开签名面 —— (et_|port_) 前缀函数/
# 类型 + 公开宏; 行首声明识别 + 块注释状态机, 不做预处理器级全展开。
#
# --diff 规则 (v2.1, "MINOR 只追加"的机检): 比较单元 = 清单条目行(markdown
# 包裹剥离), **新增 = OK; 删除/修改签名(含宏值变更) = 红**; `ET_VERSION*`
# 版本元数据宏随版本必然变化、不属冻结面, 比较时忽略。结构体字段追加在
# apidump 可见范围之外(只登记类型名), 须在交付文档 diff 说明区人工登记 ——
# 规则见 docs/API_STABILITY.md 附则。
# =====================================================================
set -u
# 字节模式固定: gawk 在 UTF-8 locale 下对含非 ASCII(如 🏠)注释行的
# C 注释正则行为漂移(v2.0 CI 首红定位), C locale 字节级匹配跨环境一致
LC_ALL=C
export LC_ALL
cd "$(dirname "$0")/.."

HDRS="et_config.h port/port.h $(ls core/*.h algorithm/*.h sys/*.h protocol/*.h drivers/*.h debug/*.h storage/*.h 2>/dev/null)"
OUT=docs/API_INVENTORY.md
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT

AWK_PROG='
    BEGIN { buf = ""; intd = 0; mf = ""; inblk = 0 }
    function norm(x) { gsub(/[ \t]+/, " ", x); sub(/^ /, "", x); sub(/ $/, "", x); return x }
    function flushbuf(   sig) {
        if (buf == "") return
        sig = norm(buf)
        sub(/;$/, "", sig)
        if (sig ~ /(^|[ \t*])(et|port)_[a-z0-9_]+[ \t]*\(/)
            print sig >> PFX ".fn"
        buf = ""
    }
    {
        line = $0
        # ---- 注释剥离(行注释 + 块注释状态机 + 行内完整注释) ----
        sub(/\/\/.*$/, "", line)
        if (inblk) {
            if (line ~ /\*\//) { sub(/^.*\*\//, "", line); inblk = 0 } else next
        }
        while (match(line, /\/\*[^*]*\*+([^/*][^*]*\*+)*\//)) {
            line = substr(line, 1, RSTART - 1) substr(line, RSTART + RLENGTH)
        }
        if (line ~ /\/\*/) { sub(/\/\*.*$/, "", line); inblk = 1 }
        if (line ~ /^[ \t]*$/) next

        # ---- 宏 (含续行合并; include guard 噪声排除) ----
        if (mf != "") {
            m = line
            if (m ~ /\\[ \t]*$/) { sub(/\\[ \t]*$/, "", m); mf = mf " " m; next }
            print norm(mf " " m) >> PFX ".mk"
            mf = ""
            next
        }
        if (line ~ /^#define[ \t]+(ET|PORT)_/ && line !~ /[A-Z0-9]+_H[ \t]*$/) {
            m = line
            sub(/^#define[ \t]+/, "", m)
            if (m ~ /\\[ \t]*$/) { sub(/\\[ \t]*$/, "", m); mf = m; next }
            print norm(m) >> PFX ".mk"
            next
        }

        # ---- 跨行拼接的声明(优先于行首过滤) ----
        if (buf != "") {
            buf = buf " " line
            if (line ~ /;/) flushbuf()
            next
        }
        # ---- typedef struct 块内 ----
        if (intd) {
            if (line ~ /\}[ \t]*[a-z_0-9]+[ \t]*;/) {
                t = line
                sub(/\}[ \t]*/, "", t)
                sub(/;.*/, "", t)
                print t >> PFX ".td"
                intd = 0
            }
            next
        }
        if (line !~ /^[a-zA-Z_]/ && line !~ /^typedef/) next
        # ---- typedef / 函数声明 ----
        if (line ~ /^typedef/) {
            if (line ~ /\{[ \t]*$/) { intd = 1; next }
            if (line ~ /\(\*[a-z_0-9]+\)/) {
                t = line
                sub(/.*\(\*/, "", t)
                sub(/\).*$/, "", t)
                print t "()" >> PFX ".td"
                next
            }
            if (line ~ /[a-z_0-9]+[ \t]*;/) {
                t = line
                sub(/;.*$/, "", t)
                sub(/.*[ \t]/, "", t)
                if (t ~ /^et_/) print t >> PFX ".td"
                next
            }
            next
        }
        if (line ~ /(^|[ \t*])(et|port)_[a-z0-9_]+[ \t]*\(/) {
            buf = line
            if (line ~ /;/) flushbuf()
            next
        }
    }
    END { if (mf != "") print norm(mf) >> PFX ".mk"; if (buf != "") flushbuf() }
'

gen_hdr() {
    h="$1"
    tag=$(printf '%s' "$h" | tr '/.' '__')
    awk -v PFX="$TMPD/$tag" "$AWK_PROG" "$h"
}

gen_body() {
    echo "# API 清单 (自动生成, 勿手改)"
    echo
    echo "> 由 \`sh tools/apidump.sh\` 生成; 一致性由 docsync \`--check\` 断言。"
    echo "> 口径: 公开签名面 —— (et_|port_) 函数声明 / typedef / 公开 ET_/PORT_ 宏(含默认值, 注释剥离)。"
    echo "> 冻结契约与上下文图例见 [API_STABILITY.md](API_STABILITY.md)。"
    for h in $HDRS; do
        [ -f "$h" ] || continue
        gen_hdr "$h"
        tag=$(printf '%s' "$h" | tr '/.' '__')
        echo
        echo "## $h"
        for part in fn td mk; do
            f="$TMPD/$tag.$part"
            [ -f "$f" ] || continue
            cnt=$(sort -u "$f" | grep -c .)
            [ "$cnt" -gt 0 ] || continue
            case "$part" in
                fn) title="函数声明" ;;
                td) title="类型" ;;
                mk) title="宏" ;;
            esac
            echo
            echo "### $title ($cnt)"
            echo
            sort -u "$f" | awk '{ print "- `" $0 "`" }'
            rm -f "$f"
        done
    done
}

# 清单条目行(去 markdown 包裹); 忽略 ET_VERSION* 版本元数据宏 —— --diff 的
# 比较单元。C locale 排序保证跨环境(Windows Git Bash / ubuntu)comm 顺序一致。
item_lines() {
    sed -n 's/^- `\(.*\)`$/\1/p' "$1" | grep -v '^ET_VERSION'
}

if [ "${1:-}" = "--check" ]; then
    TMPF=$(mktemp)
    gen_body > "$TMPF"
    if diff -q "$TMPF" "$OUT" >/dev/null 2>&1; then
        echo "apidump: 清单与头文件一致 ($OUT)"
        rm -f "$TMPF"
        exit 0
    fi
    echo "apidump: FAIL —— 公开面漂移, 清单未重新生成 (新增/变更 API 未登记即红):"
    diff -u "$OUT" "$TMPF" | head -40
    rm -f "$TMPF"
    exit 1
fi

if [ "${1:-}" = "--snapshot" ]; then
    SNAP="${2:-}"
    if [ -z "$SNAP" ]; then
        echo "用法: sh tools/apidump.sh --snapshot <清单文件>"; exit 2
    fi
    gen_body > "$SNAP"
    echo "apidump: 冻结基线已归档 $SNAP ($(item_lines "$SNAP" | grep -c .) 项)"
    exit 0
fi

if [ "${1:-}" = "--diff" ]; then
    BASE="${2:-docs/API_INVENTORY_v2.0.md}"
    if [ ! -f "$BASE" ]; then
        echo "apidump: FAIL —— 冻结基线不存在: $BASE (先 --snapshot 归档)"; exit 1
    fi
    TMPN=$(mktemp)
    gen_body > "$TMPN"
    item_lines "$BASE" | sort > "$TMPD/base.items"
    item_lines "$TMPN" | sort > "$TMPD/new.items"
    ADDED=$(comm -13 "$TMPD/base.items" "$TMPD/new.items")
    GONE=$(comm -23 "$TMPD/base.items" "$TMPD/new.items")
    NA=$(printf '%s\n' "$ADDED" | grep -c .)
    NG=$(printf '%s\n' "$GONE" | grep -c .)
    echo "apidump --diff: 基线 $BASE ↔ 当前头文件"
    echo "  新增     $NA 项  (只追加 = OK)"
    echo "  修改/删除 $NG 项  (冻结面破坏 = 红)"
    if [ "$NA" -gt 0 ]; then
        printf '%s\n' "$ADDED" | grep -v '^$' | head -40 | sed 's/^/    + /'
    fi
    if [ "$NG" -gt 0 ]; then
        printf '%s\n' "$GONE" | grep -v '^$' | head -40 | sed 's/^/    ! /'
    fi
    echo "  注: 结构体字段追加 apidump 不可见, 须在交付文档 diff 说明区登记。"
    rm -f "$TMPN"
    if [ "$NG" -gt 0 ]; then
        echo "apidump: FAIL —— 存在删改项, 违反 MINOR 只追加契约"; exit 1
    fi
    echo "apidump: OK —— 纯新增 ($NA 项, 修改/删除 0)"
    exit 0
fi

gen_body > "$OUT"
echo "apidump: 已生成 $OUT"
