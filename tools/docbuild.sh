#!/bin/sh
# docbuild.sh —— 文档命令可执行化门 (v1.9 P1-3)
#
# 提取 port/*/README.md 中 ```docbuild 定界代码块, 原样执行 (含 objcopy/size)。
# 治理目标: "文档路径断裂"类缺陷 (v1.8 验收遗留①: README 命令缺宏/缺源文件,
# CI 因自带显式清单而绿, 用户照抄即挂) 从此由 CI 直接红。
#
# 用法: sh tools/docbuild.sh [仓库根]      (默认当前目录)
# 约定: 新增 port/README 必须同批把其构建块标为 ```docbuild 并加入本清单。

set -e
ROOT="${1:-.}"
cd "$ROOT"

PORTS="port/stm32f103/README.md port/stm32g474/README.md"
DB_SCRIPT="$(mktemp)"
trap 'rm -f "$DB_SCRIPT"' EXIT
FAIL=0

for f in $PORTS; do
    if [ ! -f "$f" ]; then
        echo "FAIL: $f 不存在"
        FAIL=1
        continue
    fi
    # 提取: ```docbuild 开启, 下一个独立 ``` 闭合; 未闭合/无块均为定界错误
    awk -v file="$f" '
        /^```docbuild[ \t]*$/ { if (inblock) { print "FAIL: " file " 嵌套 docbuild 块 (文档定界错误)" > "/dev/stderr"; exit 1 } inblock=1; n++; next }
        inblock && /^```[ \t]*$/ { inblock=0; closed++; next }
        inblock { print }
        END {
            if (inblock) { print "FAIL: " file " docbuild 块未闭合 (文档定界错误)" > "/dev/stderr"; exit 1 }
            if (n+0 < 1)  { print "FAIL: " file " 无 docbuild 定界块 (文档定界错误)" > "/dev/stderr"; exit 1 }
            if (closed+0 != n+0) { print "FAIL: " file " 定界不配对 (文档定界错误)" > "/dev/stderr"; exit 1 }
        }
    ' "$f" > "$DB_SCRIPT" || { FAIL=1; continue; }

    # 多块时逐块执行: DB_SCRIPT 是整段提取 (本仓库每文件单块, 块间以空行自然分隔)
    echo "== docbuild: $f"
    if sh -e "$DB_SCRIPT"; then
        echo "== OK: $f 构建命令原样可执行"
    else
        echo "FAIL: $f 构建命令执行失败"
        FAIL=1
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "docbuild: FAILED"
    exit 1
fi
echo "docbuild: all port README commands executed clean"
