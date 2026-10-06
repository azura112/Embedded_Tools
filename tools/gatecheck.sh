#!/bin/sh
# =====================================================================
# gatecheck.sh —— ⑧ 定稿后终态复测·封闭枚举机读工具 (v2.22 G0-3, CO-6/CO-7(v2.21);
#                 v2.23 G0-2 扩展 --emit/--verify, CO-5(v2.22))
#
# 背景: ⑧"定稿后终态复测"的复测对象于 v2.21 G0-2 固化为四类封闭清单
#       ((a) §2 仓库状态类值 / (b) §4 diffstat 三数 / (c) G0 窗口最终右端 /
#       (d) 登记链放行判定)，但取值仍由人手执行——v2.17/v2.19/v2.20/v2.21
#       四版连续人工取值失准(末轮失准发生在机制留痕内)，故 v2.22 裁决提级
#       脚本化(最小形态)：机读取值 + 对照表输出，交付文档 §7.4⑧ 引其输出。
#
# v2.23 扩展 (CO-5(v2.22) 并取裁决 ②+③+①): v2.22 实测证明"取值"机读化后
#       "引用"仍是人工誊抄且错两类(1514 vs 1513 / 25 份 vs 26 份)——失准链
#       由取值迁移到引用，故把引用环节一并纳入机检：
#       --emit   按 §7.4⑧ 行文模板生成可粘贴片段(固定锚 [gatecheck:emit])，
#                交付文档 §7.4⑧ 逐字粘贴(禁重打——⑧ 引用纪律)；
#       --verify 从交付文档抽取锚行与本轮实测对账，不一致 → FAIL exit 1
#                并点名(verify 红即违 ⑧)；
#       登记行 token 计数机读输出 (CO-4(v2.22) 防再错)。
#
# 覆盖面(封闭清单的机读部分; 版本特有项——G0 窗口/逐值断言——按条文由
#       §7.4⑧ 行写明取值命令原文、人工执行对账，本工具不枚举):
#   (b) §4 diffstat 三数 —— 双口径: 全量含本文档 + 自排除交付文档
#       (口径歧义消解: v2.21 CO-1 实证两口径并存须并列输出);
#   (d) 登记链放行判定 —— 全集逐文档 docref 判定清单
#       (逐文档形态, v2.21 CO-5 裁定; 聚合行废止) + 分类计数;
#   (a) 全局机械项 —— CHANGELOG 当版节日期行 + 全文日期出现计数
#       (CO-2(v2.21) "节内 = 0"限定口径的机读形态);
#   token —— 各交付文档修订登记散文行 @token 计数(仅列 >0 者)。
#
# 用法: sh tools/gatecheck.sh <主.次版本号>              (默认模式, v2.22 形态)
#       sh tools/gatecheck.sh <主.次版本号> --emit       (生成 §7.4⑧ 可粘贴片段)
#       sh tools/gatecheck.sh <主.次版本号> --verify <交付文档路径>
#       量测区间 = snap-<主.次>-start..HEAD (与 §4 记录范围同基)。
# 自证: v2.22 发版点(tag v2.22 = d025b4c)实测 ground truth =
#       (b) 全量 19/1513/125 + 自排除 18/1346/125;
#       (d) 26 份 → OK 3 / 未登记实质变更 14 / 自指滞后 3 / 缺声明行 6 / 其他 0;
#       token = v2.20 文档 9 + v2.21 文档 4 (v2.22 文档无登记行);
#       (a) [2.22.0] = 2026-10-03。
# 性质: 独立新增工具(v2.22)的计划内演进(v2.23)——不改变任何既有门
#       (docsync/docref/apidump/sizecheck)的行为; --verify 红即不放行 tag(⑧)。
# =====================================================================
set -u
cd "$(dirname "$0")/.."

usage() {
    echo "用法: sh tools/gatecheck.sh <主.次版本号> [--emit | --verify <交付文档路径>] (如 2.21)"
}

V="${1:-}"
MODE="${2:-run}"
VERIFY_DOC="${3:-}"
case "$MODE" in
    run|--emit|--verify) ;;
    *) echo "gatecheck: 未知模式: $MODE"; usage; exit 2 ;;
esac
if [ -z "$V" ]; then usage; exit 2; fi
if [ "$MODE" = "--verify" ] && [ -z "$VERIFY_DOC" ]; then
    echo "gatecheck: FAIL —— --verify 需要第三参数: 交付文档路径"; exit 2
fi
if [ "$MODE" = "--verify" ] && [ ! -f "$VERIFY_DOC" ]; then
    echo "gatecheck: FAIL —— 交付文档不存在: $VERIFY_DOC"; exit 2
fi
SNAP="snap-${V}-start"
if ! git rev-parse --verify --quiet "${SNAP}^{commit}" >/dev/null 2>&1; then
    echo "gatecheck: FAIL —— 快照不存在: $SNAP"; exit 2
fi

# ---------- 机读取值 ----------
# (b) 双口径 diffstat —— 规格化 "N files / M / K" (与 §7.4⑧ 行文形态一致)
fmt_stat() {  # $1 = range; $2.. = 可选 pathspecs (v2.25 P1-2②: 双排除并列)
    range="$1"; shift
    if [ $# -ge 1 ]; then
        s=$(git diff --shortstat "$range" -- "$@")
    else
        s=$(git diff --shortstat "$range")
    fi
    f=$(printf '%s' "$s" | sed -n 's/^ *\([0-9][0-9]*\) files\? changed.*/\1/p')
    i=$(printf '%s' "$s" | sed -n 's/.*, \([0-9][0-9]*\) insertion.*/\1/p')
    d=$(printf '%s' "$s" | sed -n 's/.*, \([0-9][0-9]*\) deletion.*/\1/p')
    f=${f:-0}; i=${i:-0}; d=${d:-0}
    printf '%s files / %s / %s' "$f" "$i" "$d"
}
EXCL=":(exclude)v${V}开发交付__*.md"
EXCL2=":(exclude)versions/v${V}/DELIVERY*.md"      # v2.25 P1-2②: versions/ 布局排除并列
B_FULL=$(fmt_stat "${SNAP}..HEAD")
B_SELF=$(fmt_stat "${SNAP}..HEAD" "$EXCL" "$EXCL2")

# (d) 全集逐文档 docref 判定清单 + 分类计数
ok_n=0; unreg_n=0; lag_n=0; nodecl_n=0; other_n=0
detail=""
for f in v2*.md versions/*/DELIVERY*.md; do     # v2.25 P1-2①: versions/ 布局并入全集
    [ -f "$f" ] || continue                     # v2.25: 空 glob 匹配(字面量)跳过——既有输入行为零变化
    case "$f" in *开发交付*|versions/*/DELIVERY*) ;; *) continue ;; esac
    out=$(sh tools/docref.sh "$f" 2>&1 | grep -E '^docref: (OK|FAIL)' | head -1)
    case "$out" in
        *OK*)          cls="OK";              ok_n=$((ok_n+1)) ;;
        *实质变更*)    cls="未登记实质变更";  unreg_n=$((unreg_n+1)) ;;
        *自指滞后*)    cls="自指滞后";        lag_n=$((lag_n+1)) ;;
        *声明行*)      cls="缺声明行";        nodecl_n=$((nodecl_n+1)) ;;
        *)             cls="其他(须逐条解释)"; other_n=$((other_n+1)) ;;
    esac
    detail="${detail}$(printf '  %-46s %s\n' "$f" "$cls")"
done
D_N=$((ok_n+unreg_n+lag_n+nodecl_n+other_n))
D_SUM="全集 ${D_N} 份 → OK ${ok_n} / 未登记实质变更 ${unreg_n} / 自指滞后 ${lag_n} / 缺声明行 ${nodecl_n} / 其他 ${other_n}"

# (a) CHANGELOG 当版节日期 + 全文日期计数
A_SECTION=$(grep -E "^## \[${V}\.0\] — " CHANGELOG.md | grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}' | head -1)
A_HIST=$(grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}' CHANGELOG.md | sort | uniq -c | sort -rn \
         | awk '{printf "%s×%s ", $2, $1}' | sed 's/ $//')

# token —— 修订登记散文行 @token 计数 (提取管线与 docref.sh P1-2 逐字同源)
TOK=""
for f in v2*.md versions/*/DELIVERY*.md; do     # v2.25 P1-2③: versions/ 布局并入扫描
    [ -f "$f" ] || continue                     # v2.25: 空 glob 匹配(字面量)跳过
    case "$f" in *开发交付*|versions/*/DELIVERY*) ;; *) continue ;; esac
    n=$(grep -vE '^[[:space:]]*\|' "$f" | awk 'BEGIN{inf=0} /^```/{inf=!inf; next} !inf' \
        | grep '修订登记' | grep -oE '@[0-9a-f]{7,40}' | wc -l)
    if [ "$n" -gt 0 ]; then
        TOK="${TOK}${f}=${n} "
    fi
done
TOK=$(printf '%s' "$TOK" | sed 's/ $//')

# ---------- 片段行 (固定锚 [gatecheck:emit]; emit 生成 / verify 对账) ----------
L_B1="[gatecheck:emit] (b) 全量 = ${B_FULL}"
L_B2="[gatecheck:emit] (b) 自排除 = ${B_SELF} (口径 = -- '${EXCL} ${EXCL2}')"
L_D="[gatecheck:emit] (d) ${D_SUM}"
L_A="[gatecheck:emit] (a) CHANGELOG [${V}.0] 节日期 = ${A_SECTION}; 全文日期计数 = ${A_HIST}"
L_T="[gatecheck:emit] 登记 token = ${TOK:-（无登记 token）}"

echo "=== gatecheck: ⑧ 定稿后终态复测·封闭枚举 (版本 ${V}; 区间 ${SNAP}..HEAD = $(git rev-parse --short HEAD); 模式 ${MODE}) ==="

if [ "$MODE" = "--verify" ]; then
    rc=0
    for line in "$L_B1" "$L_B2" "$L_D" "$L_A" "$L_T"; do
        if ! grep -qF -- "$line" "$VERIFY_DOC"; then
            key=$(printf '%s' "$line" | sed 's/^\[gatecheck:emit\] //; s/ = .*//')
            doc_line=$(grep -F -- "[gatecheck:emit]" "$VERIFY_DOC" | grep -F -- "$key" | head -1 | tr -d '\r')
            echo "gatecheck: verify FAIL —— 片段行不一致 [${key}]:"
            echo "  期望(本轮机读): $line"
            echo "  文档实际: ${doc_line:-<缺失>}"
            rc=1
        fi
    done
    doc_anchors=$(grep -F -- "[gatecheck:emit]" "$VERIFY_DOC" || true)
    if [ -n "$doc_anchors" ]; then
        printf '%s\n' "$doc_anchors" | while IFS= read -r dl; do
            dl=$(printf '%s' "$dl" | tr -d '\r')
            hit=0
            for line in "$L_B1" "$L_B2" "$L_D" "$L_A" "$L_T"; do
                if [ "$dl" = "$line" ]; then hit=1; break; fi
            done
            if [ "$hit" -eq 0 ]; then
                echo "gatecheck: verify 注意 —— 文档含非本轮锚行(陈旧/多余, 逐条解释面): $dl"
            fi
        done
    else
        echo "gatecheck: verify FAIL —— 文档无 [gatecheck:emit] 锚行(片段未粘贴或形态漂移)"
        rc=1
    fi
    if [ "$rc" -eq 0 ]; then
        echo "gatecheck: verify PASS —— $VERIFY_DOC 与本轮机读值逐行一致 (⑧ 引用机检绿)"
    else
        echo "gatecheck: verify FAIL —— doc↔log 对账不通过 (verify 红即违 ⑧, 不放行 tag)"
    fi
    exit "$rc"
fi

# ---- (b) §4 diffstat 三数 (双口径并列) ----
echo "--- (b) §4 diffstat 三数 (双口径) ---"
echo "[全量含本文档]   ${B_FULL}"
echo "[自排除交付文档] ${B_SELF}"
echo "(两口径并列输出为 ⑧ 条文规定形态; §4 声称 = 全量口径)"

# ---- (d) 登记链放行判定 (全集逐文档 docref 判定清单) ----
echo "--- (d) 登记链放行判定 (全集逐文档 docref 判定清单) ---"
printf '%s' "$detail"
echo "  [汇总] ${D_SUM}"
if [ "$other_n" -gt 0 ]; then
    echo "  gatecheck: 注意 —— 存在'其他'类判定, 须逐条解释 (⑧ 封闭枚举四类之外的形态)"
fi

# ---- (a) 全局机械项: CHANGELOG 节日期行枚举 + 全文日期计数 ----
echo "--- (a) 全局机械项: CHANGELOG 节日期行枚举 ---"
grep -nE '^## \[[0-9]+\.[0-9]+\.[0-9]+\] — [0-9]{4}-[0-9]{2}-[0-9]{2}' CHANGELOG.md | sed 's/ ·.*//'
echo "  [全文日期出现计数] ${A_HIST}"
echo "  (节内残留核对口径: 各节 header 日期应与该节 Fixed 段的实际提交日一致)"

# ---- token 计数 (CO-4(v2.22) 机读) ----
echo "--- token 计数 (修订登记散文行 @token) ---"
echo "  [汇总] ${TOK:-（无登记 token）}"

if [ "$MODE" = "--emit" ]; then
    echo "--- ⑧ 机读值片段 (--emit 生成; 逐字粘贴至交付文档 §7.4⑧, 禁重打) ---"
    printf '%s\n' "$L_B1" "$L_B2" "$L_D" "$L_A" "$L_T"
    echo "--- 片段结束 ---"
fi
echo "=== gatecheck 完 ==="
