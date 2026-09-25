#!/bin/sh
# =====================================================================
# docref.sh —— 交付文档 §4 自指核验 (v2.6 P0-3; CO-5 机制化)
#
# 问题(v2.5 评审 D-10): 交付文档 §4 的 commit/diffstat 由人工回填,
# 与实测会滞后自身提交(写作时点 ≠ 记录终点), 且无门守护。
#
# 本脚本把 §4 的"记录范围"变成机器核验的硬约束 —— 交付文档 §4 必须含一行:
#
#   `git diff --stat <base>..<end>` = **N files changed, M insertions(+), K deletions(-)**
#
# 其中 <base> 为快照 tag(如 `snap-2.7-start`), <end> 为**记录终点**;
# 三个数字须与 `git diff --stat <base>..<end>` 实测**逐值相等**(`<base>`/`<end>`
# 亦须可解析)。不符即红 —— 禁止"取自回填前提交"的自指滞后。
#
# **记录终点必须覆盖到 HEAD** (v2.6-r2 G0′-6, `CO-4(v2.6)`):
#   v2.6 的空隙 = `<end>` 由作者自选为"最后一次**代码**提交", 其后**任意数量的文档
#   提交不受检** —— 该版最严重的两处文本错误(CHANGELOG 挂账段不实、交付文档自相矛盾)
#   恰好全部落在这个盲区。现要求 `<end>` 解析后**等于 HEAD**(或直接写 `HEAD`):
#   任何"记录范围之外的提交"一律使门变红, 迫使 §4 随终态一起回填。
#
# 用法: sh tools/docref.sh [交付文档路径]
#       默认取当前版本 `v<主.次>开发交付*.md`。
# CI 不跑(浅检出无 tag 历史), 属**发布前本机门** —— 沿 sizecheck "CI 只上传读数"
# 先例; 调用时机见 README 发布 checklist。
#
# 自证可红: ① 把 §4 的任一个数字改 1 → 退出非零; ② 在 `<end>` 之后追加一个纯文档
# 提交(不重新回填 §4) → 退出非零("记录终点未覆盖 HEAD")—— 交付文档 §7 机制自证表。
# =====================================================================
set -u

cd "$(dirname "$0")/.."

VMM=$(gcc -E -P -dM et_config.h 2>/dev/null | awk '/define ET_VERSION_MAJOR/{maj=$3}
                                                  /define ET_VERSION_MINOR/{min=$3}
                                                  END{print maj"."min}')
if [ -z "${VMM:-}" ]; then
    echo "docref: FAIL —— 无法从 et_config.h 解析版本号 (需要 gcc 预处理器)"
    exit 2
fi

DOC="${1:-}"
if [ -z "$DOC" ]; then
    DOC=$(ls v${VMM}开发交付*.md 2>/dev/null | head -1)
fi
if [ -z "${DOC:-}" ] || [ ! -f "$DOC" ]; then
    echo "docref: FAIL —— 未找到当前版本交付文档 v${VMM}开发交付*.md (可用参数显式指定)"
    exit 1
fi

line=$(grep -oE 'git diff --stat [^`]+` = \*\*[0-9]+ files? changed, [0-9]+ insertions?\(\+\), [0-9]+ deletions?\(-\)\*\*' "$DOC" | head -1)
if [ -z "${line:-}" ]; then
    echo "docref: FAIL —— $DOC 缺'记录范围 + diffstat'声明行"
    echo "       期望形态: \`git diff --stat <base>..<end>\` = **N files changed, M insertions(+), K deletions(-)**"
    exit 1
fi

RANGE=$(printf '%s' "$line" | sed -n 's/.*git diff --stat \([^`]*\)`.*/\1/p')
D_FILES=$(printf '%s' "$line" | sed -n 's/.*\*\*\([0-9][0-9]*\) files.*/\1/p')
D_INS=$(printf '%s' "$line" | sed -n 's/.*, \([0-9][0-9]*\) insertions.*/\1/p')
D_DEL=$(printf '%s' "$line" | sed -n 's/.*, \([0-9][0-9]*\) deletions.*/\1/p')

BASE="${RANGE%%..*}"
END="${RANGE##*..}"
if [ "$BASE" = "$RANGE" ] || [ -z "${BASE:-}" ] || [ -z "${END:-}" ]; then
    echo "docref: FAIL —— 记录范围形态不合法: [$RANGE] (期望 <base>..<end>)"
    exit 1
fi
if ! git rev-parse --verify --quiet "${BASE}^{commit}" >/dev/null 2>&1; then
    echo "docref: FAIL —— 记录范围起点不存在: $BASE"
    exit 1
fi
if ! git rev-parse --verify --quiet "${END}^{commit}" >/dev/null 2>&1; then
    echo "docref: FAIL —— 记录范围终点不存在: $END"
    exit 1
fi

# G0′-6 / CO-4(v2.6): 记录终点必须覆盖到 HEAD —— 禁止把 <end> 取成"最后一次代码
# 提交"从而让其后任意数量的文档提交落在门的视野之外(本版两处最严重文本错误即在此盲区)。
END_SHA=$(git rev-parse --verify --quiet "${END}^{commit}")
HEAD_SHA=$(git rev-parse --verify --quiet "HEAD^{commit}")
if [ "$END_SHA" != "$HEAD_SHA" ]; then
    echo "docref: FAIL —— 记录终点未覆盖 HEAD ($RANGE 自指滞后)"
    echo "       声明 END = $(git rev-parse --short "$END_SHA") ($END)"
    echo "       实测 HEAD = $(git rev-parse --short "$HEAD_SHA") ($(git rev-parse --abbrev-ref HEAD 2>/dev/null))"
    echo "       修法: 把 §4 的记录范围终点写为 HEAD(或回填至当前 HEAD 的等价表述)后重跑。"
    exit 1
fi

ACTUAL=$(git diff --stat "$RANGE" 2>/dev/null | tail -1)
if [ -z "${ACTUAL:-}" ]; then
    echo "docref: FAIL —— git diff --stat $RANGE 无输出"
    exit 1
fi
A_FILES=$(printf '%s' "$ACTUAL" | sed -n 's/^ *\([0-9][0-9]*\) files\? changed.*/\1/p')
A_INS=$(printf '%s' "$ACTUAL" | sed -n 's/.*, \([0-9][0-9]*\) insertions\?(+)\(.*\)\?.*/\1/p')
A_DEL=$(printf '%s' "$ACTUAL" | sed -n 's/.*, \([0-9][0-9]*\) deletions\?(-).*/\1/p')
if [ -z "${A_FILES:-}" ]; then
    A_FILES=0                     # "1 file changed" 单数/纯二进制等形态兜底
fi
if [ -z "${A_INS:-}" ]; then
    A_INS=0
fi
if [ -z "${A_DEL:-}" ]; then
    A_DEL=0
fi

echo "docref: 交付文档 $DOC"
echo "  记录范围 $RANGE   (实测: $ACTUAL)"
echo "  文档声明: $D_FILES files / $D_INS insertions / $D_DEL deletions"
echo "  实测读数: $A_FILES files / $A_INS insertions / $A_DEL deletions"

if [ "$D_FILES" = "$A_FILES" ] && [ "$D_INS" = "$A_INS" ] && [ "$D_DEL" = "$A_DEL" ]; then
    echo "docref: OK —— §4 的 commit 范围与 diffstat 与实测逐值一致"
    exit 0
fi

echo "docref: FAIL —— §4 声明与实测不符 (自指滞后)"
exit 1
