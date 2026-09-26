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
# **记录终点必须覆盖到 HEAD** (v2.6-r2 G0′-6, `CO-4(v2.6)`) —— v2.8 P1-2 起迭代为
# **白名单式判定** (`CO-6(v2.7)`, AC-8 三场景矩阵):
#   交付文档把记录终点**冻结**为字面 SHA 或 tag 符号(如 `..v2.8`)后:
#   · END == HEAD(交付时点) → 经典路径, diffstat 三数字严格对账(不变);
#   · END < HEAD(交付后)   → 逐提交分类 <END>..HEAD:
#       - 仅触及**良性文件**(评审记录/实机记录/各版计划与交付文档/两模板/
#         v3-candidates) → 放行, 打印"N 个提交未计入 diffstat"提示;
#       - 触及 CHANGELOG.md 或本文档 → 须有**修订登记**(文档内含"修订登记"+该提交
#         SHA 的行), 或该提交对本文档的增删行**全部**为登记行(纯登记提交);
#       - 触及其它文件(代码/工具/README 等) → 未经登记的实质变更, FAIL。
#   旧式声明(终点写符号 `HEAD`)保持原严格语义: 终点随运行时 HEAD 漂移, 数字必须
#   随时对账 —— 历史文档行为不变, 新交付文档一律用冻结终点。
#
# **清单逐 SHA 可达性** (v2.8 P1-1, `CO-1(v2.7)` 装门半): §4 的 commit 清单(逐 SHA
# 列表形态)中每个 SHA 必须满足 `git merge-base --is-ancestor <sha> <END>`(RANGE
# 可达); 悬空/不可达 → FAIL 并列出。清单不含交付文档定稿提交自身(自指不可预写,
# 由终点校验覆盖)。旧文档不回溯校验(沿 docsync"历史文档引用不在校验范围"先例)。
#
# 用法: sh tools/docref.sh [交付文档路径]
#       默认取当前版本 `v<主.次>开发交付*.md`。
# CI 不跑(浅检出无 tag 历史), 属**发布前本机门** —— 沿 sizecheck "CI 只上传读数"
# 先例; 调用时机见 README 发布 checklist。
#
# 自证可红: ① 把 §4 的任一个数字改 1 → 退出非零; ② 清单写入悬空 SHA → 退出非零
# 并列出; ③ 冻结终点后构造触及 CHANGELOG/本文档的未登记提交 → 退出非零
# —— 交付文档 §7 机制自证表。
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

# G0′-6 / CO-4(v2.6) 原硬判据, v2.8 P1-2 迭代为白名单式判定(CO-6(v2.7)) —— 见文件头。
END_SHA=$(git rev-parse --verify --quiet "${END}^{commit}")
HEAD_SHA=$(git rev-parse --verify --quiet "HEAD^{commit}")

# ---- P1-1 (CO-1(v2.7) 装门半): §4 清单逐 SHA 可达性 ----
DOC_REL=$(printf '%s' "$DOC" | sed 's|^\./||')
sec4=$(awk '/^## 4[.、]/{f=1; next} /^## [0-9]/{f=0} f' "$DOC_REL")
bad_shas=""
for sha in $(printf '%s\n' "$sec4" | sed -n 's/^[[:space:]]*>\?[[:space:]]*\([0-9a-f]\{7,40\}\)[[:space:]].*/\1/p'); do
    if ! git rev-parse --verify --quiet "${sha}^{commit}" >/dev/null 2>&1; then
        bad_shas="$bad_shas $sha(悬空/不存在)"
    elif ! git merge-base --is-ancestor "$sha" "$END_SHA" 2>/dev/null; then
        bad_shas="$bad_shas $sha(终点不可达)"
    fi
done
if [ -n "$bad_shas" ]; then
    echo "docref: FAIL —— §4 清单含记录终点($END)不可达的 SHA:$bad_shas"
    echo "       悬空提交不得作为记录范围成员(v2.7 CO-1 的门空隙); 修正清单后重跑。"
    exit 1
fi

# ---- P1-2 (CO-6(v2.7)): 白名单式判定 ----
if [ "$END_SHA" != "$HEAD_SHA" ]; then
    # 修订登记集: 文档内含"修订登记"且带 7~40 位十六进制(小写)的行 → 提取 SHA。
    # 边界要求两侧非十六进制, 防 sha256(64 位)被截取误配。
    reg_shas=$(grep '修订登记' "$DOC_REL" | grep -oE '(^|[^0-9a-f])[0-9a-f]{7,40}($|[^0-9a-f])' \
              | sed 's/[^0-9a-f]//g' | sort -u)
    n_after=0
    unreg=""
    while IFS= read -r c; do
        [ -n "$c" ] || continue
        n_after=$((n_after + 1))
        cshort=$(git rev-parse --short "$c")
        cdesc=$(git log -1 --format='%h %s' "$c")
        cfiles=$(git -c core.quotepath=off diff-tree --no-commit-id --name-only -r "$c")
        registered=0
        if [ -n "$reg_shas" ] && printf '%s\n' "$reg_shas" | grep -q "^${cshort}"; then
            registered=1
        fi
        if [ "$registered" -eq 1 ]; then
            continue
        fi
        touches_doc=$(printf '%s\n' "$cfiles" | grep -Fxc -- "$DOC_REL")
        touches_cl=$(printf '%s\n' "$cfiles" | grep -Fxc -- "CHANGELOG.md")
        if [ "${touches_doc:-0}" -eq 1 ] && [ "${touches_cl:-0}" -eq 0 ]; then
            # 纯登记提交: 对本文档的增删行全部含"修订登记"
            if git diff-tree --no-commit-id -p -r "$c" -- "$DOC_REL" \
               | grep -E '^[+-]' | grep -vE '^(\+\+\+|---)' \
               | grep -qvE '修订登记'; then
                : # 存在非登记行 → 非纯登记
            else
                continue
            fi
        fi
        benign=1
        for f in $cfiles; do
            case "$f" in
                *评审记录*.md|*实机记录*.md|v[0-9]*开发计划*.md|v[0-9]*开发交付*.md|docs/评审记录模板.md|docs/开发计划模板.md|docs/v3-candidates.md) ;;
                *) benign=0; break ;;
            esac
        done
        if [ "$benign" -eq 1 ] && [ "${touches_doc:-0}" -eq 0 ] && [ "${touches_cl:-0}" -eq 0 ]; then
            continue
        fi
        unreg="$unreg
  $cdesc"
    done <<EOF
$(git rev-list --reverse "$END_SHA..$HEAD_SHA")
EOF
    if [ -n "$unreg" ]; then
        echo "docref: FAIL —— <END>..HEAD 存在未经修订登记的实质变更(反滞后意图保留):"
        printf '%s\n' "$unreg"
        echo "       触及 CHANGELOG/交付文档或非良性文件的提交, 须在交付文档以'修订登记 @<sha>'"
        echo "       登记后由登记提交放行(AC-8 场景 B/C); 或把记录终点回填至当前 HEAD。"
        exit 1
    fi
    if [ "$n_after" -gt 0 ]; then
        echo "docref: 提示 —— <END>($END) 后有 $n_after 个提交未计入 diffstat(清单):"
        git log --oneline --format='  %h %s' "$END_SHA..$HEAD_SHA"
    fi
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
