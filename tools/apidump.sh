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
#   sh tools/apidump.sh --check                # 与已入库清单比对(**行尾归一后**), 漂移非零
#   sh tools/apidump.sh --snapshot <清单文件>  # 固化冻结基线 (v2.1 P0-1)
#   sh tools/apidump.sh --diff [基线清单]      # 与基线比对: 纯新增=绿 (默认
#                                              #   docs/API_INVENTORY_v2.7.md)
#
# --check 的行尾口径 (v2.6-r2 G0′-1, `CO-1(v2.6)`):
#   本仓 `core.autocrlf=true` 且无 `.gitattributes` → 入库清单在工作树被检出为 **CRLF**,
#   而 gen_body 产出 **LF** ⇒ 旧实现的字节级 `diff -q` **恒报"公开面漂移"**(与 API 面
#   无关), 并级联把 docsync 打成 fail=2。修法 = 比对前把两侧行尾统一为 LF 后比对
#   **完整内容**(分节表头/计数一并比对, 判别力与字节比对等价); 不依赖工作树行尾,
#   故 Windows(CRLF 检出) 与 Linux(LF 检出) 行为一致。`--diff`/`--snapshot` 走
#   `item_lines`(仅提取条目行), 对行尾天然免疫 —— 根因: GNU sed 读入时剥离行尾 CR。
# 基线滚动规则 (v2.2 P0-1): 默认基线 = 最近已发布 MINOR 的快照; 发版时用
#   --snapshot 归档新基线后把此处默认值前滚; 旧基线文件只读保留(历史审计)。
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
# Windows/MSYS 下 TEMP 常为 "C:\...\Temp" 形式: gawk 内部的 `>> PFX ".fn"` 会把
# 路径里的反斜杠当转义序列 → 提取文件写到别处 → 清单比对**误报漂移**(v2.6 本机
# 实测)。Windows 分支改用**仓库内相对路径**(build/ 已在 .gitignore): 无盘符、
# 无反斜杠, gawk 与 rm/diff 两侧语义一致。Linux 无 cygpath → 保持 mktemp -d,
# CI 行为完全不变。
if command -v cygpath >/dev/null 2>&1; then
    TMPD_REL="build/.apidump.$$"
    if mkdir -p "$TMPD_REL" 2>/dev/null; then
        TMPD="$TMPD_REL"
    else
        # v2.8 P1-4 (CO-9(v2.7)): 回退不得静默 —— 旧版"路径转义 → gawk 提取为空"陷阱
        # 的前提就是 Windows 分支失效而无任何提示(v2.5 时代工具在 worktree 内生成
        # 0 条目, 交付 N-3 实测)。告警不改变退出码, 只提示排查 mkdir 失败原因。
        echo "apidump: WARN —— 仓库内临时目录 $TMPD_REL 创建失败, 回退 mktemp -d;" \
             "若随后提取为空/比对全漂移, 先排查 build/ 可写性与路径转义 (CO-9(v2.7))" >&2
    fi
fi
# 清理失败不得影响门的结论(受限 shell 封装下 rm 会失败/需确认 —— 见上 rm 说明):
# 临时目录残留于 build/(已 gitignore), 下次运行会被覆盖, 不影响正确性。
trap 'rm -rf "$TMPD" 2>/dev/null || true' EXIT

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
            # 临时产物不在此处逐个删除: 统一由 trap 清理 TMPD(v2.6 本机实测:
            # 逐文件 rm 在某些 shell 封装(安全删除 wrapper)下每次耗时数十秒,
            # 108 次调用使脚本从秒级劣化到小时级; 一次 rm -rf 即可)
        done
    done
}

# 清单条目行(去 markdown 包裹); 忽略 ET_VERSION* 版本元数据宏 —— --diff 的
# 比较单元。C locale 排序保证跨环境(Windows Git Bash / ubuntu)comm 顺序一致。
# 显式 `tr -d '\r'`(v2.8 P1-3, CO-8(v2.7)): 把"GNU sed 读文件时剥 CR 的隐式副作用"
# 升级为契约 —— 管道输入不经文本模式转换, CRLF 经管道进入模式空间会使 `$` 锚定
# 落空 → 0 条目 → --diff 误红; 先剥 CR 后不再依赖任何实现的隐式行为。
item_lines() {
    tr -d '\r' < "$1" | sed -n 's/^- `\(.*\)`$/\1/p' | grep -v '^ET_VERSION'
}

if [ "${1:-}" = "--check" ]; then
    TMPF="$TMPD/gen_check.md"       # 放 TMPD 内, 由 trap 统一清理(见上 rm 说明)
    gen_body > "$TMPF"
    # 行尾归一化后比对完整内容(G0′-1 / CO-1(v2.6)): 归一只作用于比对副本,
    # 不触碰工作树文件 —— `sed 's/\r$//'` 在 LF 侧为恒等变换, 故跨环境一致。
    sed 's/\r$//' "$TMPF" > "$TMPD/chk_gen.txt"
    sed 's/\r$//' "$OUT"  > "$TMPD/chk_cur.txt"
    if diff -q "$TMPD/chk_gen.txt" "$TMPD/chk_cur.txt" >/dev/null 2>&1; then
        echo "apidump: 清单与头文件一致 ($OUT)"
        exit 0
    fi
    echo "apidump: FAIL —— 公开面漂移, 清单未重新生成 (新增/变更 API 未登记即红):"
    diff -u "$TMPD/chk_cur.txt" "$TMPD/chk_gen.txt" | head -40
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
    BASE="${2:-docs/API_INVENTORY_v2.7.md}"    # 滚动规则: 默认 = 最近已发布 MINOR 快照 (v2.8 P0-5 发版点: v2.6→v2.7; P0-2 已清偿 CO-2(v2.7) 滞留)
    if [ ! -f "$BASE" ]; then
        echo "apidump: FAIL —— 冻结基线不存在: $BASE (先 --snapshot 归档)"; exit 1
    fi
    TMPN="$TMPD/gen_new.md"         # 同上: 由 trap 统一清理
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
    if [ "$NG" -gt 0 ]; then
        echo "apidump: FAIL —— 存在删改项, 违反 MINOR 只追加契约"; exit 1
    fi
    echo "apidump: OK —— 纯新增 ($NA 项, 修改/删除 0)"
    exit 0
fi

# ---- v2.8 P1-4 (CO-9(v2.7)): 生成条目数守卫 —— 防残缺生成静默覆盖清单 ----
# 参照 = 现有工作清单与最近归档中的较大者(阈值 ½, 可裁量项 6: 沿交付 N-3 建议);
# 生成条目数不足参照一半 → 拒绝写入 + exit 1 + 诊断。首次生成(无任何参照)不拦。
GEN="$TMPD/gen_default.md"
gen_body > "$GEN"
new_n=$(item_lines "$GEN" | grep -c .)
ref_n=0; ref_src="(无参照, 守卫跳过)"
if [ -f "$OUT" ]; then
    ref_n=$(item_lines "$OUT" | grep -c .); ref_src="$OUT"
fi
latest=$(ls docs/API_INVENTORY_v*.md 2>/dev/null | sort -V | tail -1)
if [ -n "${latest:-}" ] && [ -f "$latest" ]; then
    a_n=$(item_lines "$latest" | grep -c .)
    if [ "$a_n" -gt "$ref_n" ]; then ref_n=$a_n; ref_src="$latest"; fi
fi
if [ "$ref_n" -gt 0 ] && [ "$new_n" -lt $((ref_n / 2)) ]; then
    echo "apidump: FAIL —— 生成条目数 $new_n < 参照清单($ref_src) $ref_n 项的一半, 拒绝写入 $OUT (CO-9(v2.7))"
    echo "       典型根因: Windows 路径转义使 gawk 提取为空 / 头文件集合异常。"
    echo "       本运行未触碰现有清单; 排查生成环境后重跑, 或用 --snapshot 显式归档。"
    exit 1
fi
mv -f "$GEN" "$OUT"
echo "apidump: 已生成 $OUT ($new_n 项)"
