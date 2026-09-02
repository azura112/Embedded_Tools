#!/bin/sh
# =====================================================================
# Renode 仿真 smoke: 跑 stm32f103_demo.elf 并断言关键串口日志 (v1.3)
#
# 本机与 CI 共用; 用法:
#   smoke.sh <renode可执行> <renode安装根目录> <仓库根目录> <输出目录>
#
# 断言项 (P1-1 验收, 见 v1.3 计划):
#   1. 版本横幅        "Embedded_Tools v..."
#   2. kv 初始化行     "kv: seq=... free=... rec=... key=..." (key>0)
#   3. 心跳行          "alive ... boot #... (UTC)"
#   4. 重启计数首启    "boot #1"
#   5. 重启计数复读    "boot #2"  (cpu Reset 后 kv 保持 → +1, 掉电持久化证据)
#
# 仿真要点 (Renode 1.16.x, 与平台差异见计划 §7):
#   - F103 flash 控制器无寄存器模型: 用 `sysbus Tag` 把 0x40022000-13
#     标记为常读 0 (BSY 恒闲/无错误位), 存储体本身可写 —— 只验功能,
#     不验擦写时序 (时序以真机为准);
#   - flash 上电前以 flash_erased_64k.bin (全 0xFF) 填充, 模拟真实擦除态;
#   - 有界运行用 `emulation RunFor` (不能先 start), 复位用 `sysbus.cpu
#     Reset` (内存保持, 等效软复位)。
# =====================================================================
set -u

RENODE="$1"
RENODE_ROOT="$2"
REPO="$3"
OUT="$4"

fail() {
    echo "[smoke] FAIL: $1" >&2
    exit 1
}

# 统一成 Renode 可接受的 D:/a/b 形式 (Git Bash cygpath; linux 原样)
norm() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        echo "$1"
    fi
}

ELF=$(norm "$REPO/build/stm32f103_demo.elf")
FILL=$(norm "$REPO/port/stm32f103/renode/flash_erased_64k.bin")
PLAT=$(norm "$RENODE_ROOT/platforms/cpus/stm32f103.repl")
LOG=$(norm "$OUT/uart.log")
RESC=$(norm "$OUT/run.resc")

[ -f "$ELF" ]  || fail "ELF 不存在: $ELF (先交叉编译 demo)"
[ -f "$FILL" ] || fail "擦除态填充文件不存在: $FILL"
[ -f "$PLAT" ] || fail "Renode 平台描述不存在: $PLAT"

mkdir -p "$OUT"
rm -f "$LOG" "$RESC" "$OUT/renode.out"

cat > "$RESC" <<EOF
using sysbus
mach create "f103"
machine LoadPlatformDescription @$PLAT
sysbus Tag <0x40022000, 0x40022013> "FLASH" 0x0
sysbus LoadBinary @$FILL 0x08000000
sysbus LoadELF @$ELF
sysbus.usart1 CreateFileBackend @$LOG
emulation RunFor "00:00:04"
sysbus.cpu Reset
emulation RunFor "00:00:04"
quit
EOF

echo "[smoke] renode: $RENODE"
echo "[smoke] script: $RESC"
echo "[smoke] uart log -> $LOG"

"$RENODE" --disable-xwt --plain -e "include @$RESC" > "$OUT/renode.out" 2>&1
RC=$?

echo "[smoke] renode exit=$RC"

# ---- 断言 ----
grep -q "Embedded_Tools v"   "$LOG" || fail "断言1 失败: 版本横幅缺失"
grep -q "kv: seq="           "$LOG" || fail "断言2 失败: kv stats 行缺失"
grep -q "boot #1"            "$LOG" || fail "断言4 失败: 首启 boot #1 缺失"
grep -q "boot #2"            "$LOG" || fail "断言5 失败: 复位后 boot #2 缺失(kv 未持久化?)"
grep -qE "alive [0-9]+ ms \| boot #[0-9]+ \| [0-9]{4}-[0-9]{2}-[0-9]{2}" \
                             "$LOG" || fail "断言3 失败: 心跳行(含 UTC 时间)缺失"
# kv 有效 key 数须 >= 2 (重启计数 + 时间戳)
grep -E "kv: seq=" "$LOG" | grep -qE "key=[2-9]" \
                             || fail "断言2b 失败: kv 有效 key 数异常"

echo "[smoke] PASS: 全部断言通过"
exit 0
