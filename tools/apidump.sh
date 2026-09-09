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
#   sh tools/apidump.sh            # 重新生成 docs/API_INVENTORY.md
#   sh tools/apidump.sh --check    # 与已入库清单比对, 漂移退出非零
# 提取口径(见计划 §7 风险对策): 公开签名面 —— (et_|port_) 前缀函数/
# 类型 + 公开宏; 行首声明识别 + 块注释状态机, 不做预处理器级全展开。
# =====================================================================
set -u
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
gen_body > "$OUT"
echo "apidump: 已生成 $OUT"
