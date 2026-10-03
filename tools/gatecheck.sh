#!/bin/sh
# =====================================================================
# gatecheck.sh —— ⑧ 定稿后终态复测·封闭枚举机读工具 (v2.22 G0-3, CO-6/CO-7(v2.21))
#
# 背景: ⑧"定稿后终态复测"的复测对象于 v2.21 G0-2 固化为四类封闭清单
#       ((a) §2 仓库状态类值 / (b) §4 diffstat 三数 / (c) G0 窗口最终右端 /
#       (d) 登记链放行判定)，但取值仍由人手执行——v2.17/v2.19/v2.20/v2.21
#       四版连续人工取值失准(末轮失准发生在机制留痕内)，故本版裁决提级
#       脚本化(最小形态)：机读取值 + 对照表输出，交付文档 §7.4⑧ 引其输出。
#
# 覆盖面(封闭清单的机读部分; 版本特有项——G0 窗口/逐值断言——按条文由
#       §7.4⑧ 行写明取值命令原文、人工执行对账，本工具不枚举):
#   (b) §4 diffstat 三数 —— 双口径: 全量含本文档 + 自排除交付文档
#       (口径歧义消解: v2.21 CO-1 实证两口径并存须并列输出);
#   (d) 登记链放行判定 —— 全集逐文档 docref 判定清单
#       (逐文档形态, v2.21 CO-5 裁定; 聚合行废止) + 分类计数;
#   (a) 全局机械项 —— CHANGELOG 各 [2.x.0] 节日期行枚举 + 全文日期
#       出现计数 (CO-2(v2.21) "节内 = 0"限定口径的机读形态)。
#
# 用法: sh tools/gatecheck.sh <主.次版本号>   (如: sh tools/gatecheck.sh 2.21)
#       量测区间 = snap-<主.次>-start..HEAD (与 §4 记录范围同基)。
# 自证: v2.21 发版点(tag v2.21 = abf0ddc)实测 ground truth =
#       (b) 全量 18/1439/131 + 自排除 17/1274/131;
#       (d) 25 份 → OK 2 / 未登记实质变更 14 / 自指滞后 3 / 缺声明行 6;
#       (a) CHANGELOG "2026-10-01" 全文 = 2 (v2.18/v2.19 节真实日期)。
# 性质: 独立新增工具——不改变任何既有门(docsync/docref/apidump/sizecheck)
#       的行为; 自身为非门面的机读取值器(输出供 §7.4⑧ 引用)。
# =====================================================================
set -u
cd "$(dirname "$0")/.."

V="${1:-}"
if [ -z "$V" ]; then
    echo "用法: sh tools/gatecheck.sh <主.次版本号> (如 2.21)"; exit 2
fi
SNAP="snap-${V}-start"
if ! git rev-parse --verify --quiet "${SNAP}^{commit}" >/dev/null 2>&1; then
    echo "gatecheck: FAIL —— 快照不存在: $SNAP"; exit 2
fi

echo "=== gatecheck: ⑧ 定稿后终态复测·封闭枚举 (版本 ${V}; 区间 ${SNAP}..HEAD = $(git rev-parse --short HEAD)) ==="

# ---- (b) §4 diffstat 三数 (双口径并列) ----
echo "--- (b) §4 diffstat 三数 (双口径) ---"
echo -n "[全量含本文档]  "; git diff --shortstat "${SNAP}..HEAD"
echo -n "[自排除交付文档] "; git diff --shortstat "${SNAP}..HEAD" -- ":(exclude)v${V}开发交付__*.md"
echo "(两口径并列输出为 ⑧ 条文规定形态; §4 声称 = 全量口径)"

# ---- (d) 登记链放行判定 (全集逐文档 docref 判定清单) ----
echo "--- (d) 登记链放行判定 (全集逐文档 docref 判定清单) ---"
ok_n=0; unreg_n=0; lag_n=0; nodecl_n=0; other_n=0
for f in v2*.md; do
    case "$f" in *开发交付*) ;; *) continue ;; esac
    out=$(sh tools/docref.sh "$f" 2>&1 | grep -E '^docref: (OK|FAIL)' | head -1)
    case "$out" in
        *OK*)          cls="OK";              ok_n=$((ok_n+1)) ;;
        *实质变更*)    cls="未登记实质变更";  unreg_n=$((unreg_n+1)) ;;
        *自指滞后*)    cls="自指滞后";        lag_n=$((lag_n+1)) ;;
        *声明行*)      cls="缺声明行";        nodecl_n=$((nodecl_n+1)) ;;
        *)             cls="其他(须逐条解释)"; other_n=$((other_n+1)) ;;
    esac
    printf "  %-46s %s\n" "$f" "$cls"
done
echo "  [汇总] 全集=$((ok_n+unreg_n+lag_n+nodecl_n+other_n)) 份 -> OK=${ok_n} / 未登记实质变更=${unreg_n} / 自指滞后=${lag_n} / 缺声明行=${nodecl_n} / 其他=${other_n}"
if [ "$other_n" -gt 0 ]; then
    echo "  gatecheck: 注意 —— 存在'其他'类判定, 须逐条解释 (⑧ 封闭枚举四类之外的形态)"
fi

# ---- (a) 全局机械项: CHANGELOG 节日期行枚举 + 全文日期计数 ----
echo "--- (a) 全局机械项: CHANGELOG 节日期行枚举 ---"
grep -nE '^## \[[0-9]+\.[0-9]+\.[0-9]+\] — [0-9]{4}-[0-9]{2}-[0-9]{2}' CHANGELOG.md | sed 's/ ·.*//'
echo "  [全文日期出现计数] (节内残留核对口径: 各节 header 日期应与该节 Fixed 段的实际提交日一致)"
grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}' CHANGELOG.md | sort | uniq -c | sort -rn | while read -r n d; do
    printf "  %s × %s\n" "$d" "$n"
done
echo "=== gatecheck 完 ==="
