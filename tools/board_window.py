#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v2.28 P3-1（REQ-5 / ROADMAP M3-⑤）—— G474 板侧窗口一键化。

把 `移植stm32实机记录.md` §16.2 手工规程（跨版本每版重复的十九行自证）压成一条命令，
五要素按序执行、任一红即中止：

  ① 端口枚举     —— `serial.tools.list_ports`，**按设备描述匹配**（不凭记录直取号）
  ② 重做同步     —— 七目录 + `et_config.h` + `port/port.h` 拷入板侧工程后逐目录递归比对
  ③ 板侧构建     —— `cmake --build --preset`，日志留痕并计 warning 数
  ④ 烧前版本串机检 —— 扫目标 ELF 内的 `x.y.z` 串集合，须含当版串，否则 **abort 不烧录**
  ⑤ 烧录 + 走单   —— `STM32_Programmer_CLI`（须见 "Download verified successfully"）
                     再经串口跑五项命令并落盘原始全文 + 摘要

三模式：`--dry-run`（只 ①②④ 的只读面 + 行动计划打印，**绝不构建/烧录/走单**）、
`--check-only`（①②③④，不烧录不走单）、`--full`（①~⑤ 全跑）。

证据形态与 §17 手工留痕同构（同目录、同名件），便于逐要素对账。

退出码语义（HC-8①，勿误读）：0 = 五要素的**上位机侧机检**全部通过；非 0 = 有要素红或
被中止。**退出码 0 不等于"板侧绿"**——板侧读数只看 `walk_output.txt` / `walk_summary.md`
里的原文行（版本串、`disc=0`、`22/22`、`METRICS`、`BOOTINFO` 三字段）。

本文件零反斜杠字面量（Windows 路径一律正斜杠形态，Python 在 Windows 下同样接受）——
沿 v2.28 D-1 勘误教训：非 raw 字符串里的 backslash 会被求值成裸控制符，破坏路径与表格。
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys
import time

NL = chr(10)
CR = bytes([13])
LF = bytes([10])
CRLF = CR + LF

REPO_DEFAULT = "D:/code/My_Library/Embedded_Tools"
BOARD_DEFAULT = "D:/code/STM32CubeMX/G474VET6_ET_TEST"
CLI_DEFAULT = ("D:/software/STM32CubeCLT_1.18.0/STM32CubeProgrammer/"
               "bin/STM32_Programmer_CLI.exe")
PORT_DESC_DEFAULT = "CH343"
SIZE_DEFAULT = "D:/software/STM32CubeCLT_1.18.0/GNU-tools-for-STM32/bin/arm-none-eabi-size.exe"
BAUD_DEFAULT = 115200

SYNC_DIRS = ("core", "algorithm", "sys", "protocol", "drivers", "storage", "debug")
MODBUS_ADDR = 0x11
VER_RE = re.compile(rb"[0-9]+\.[0-9]+\.[0-9]+")   # bytes 形态: 机检面扫的是 ELF 原始字节


def now():
    return time.strftime("%Y-%m-%d %H:%M:%S")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


class Window:
    def __init__(self, args):
        self.repo = args.repo
        self.board = args.board
        self.cli = args.cli
        self.desc = args.desc
        self.baud = args.baud
        self.preset = args.preset
        self.size_cli = getattr(args, "size_cli", SIZE_DEFAULT)
        self.mode = args.mode
        self.lib_ver = self.read_lib_version()
        self.evname = args.evidence or ("v" + self.lib_ver.rsplit(".", 1)[0] + "-evidence")
        self.evdir = os.path.join(self.board, "build", self.evname)
        self.lines = []
        self.abort_reason = None

    # ---------- 留痕 ----------
    def log(self, text):
        stamp = time.strftime("%H:%M:%S")
        self.lines.append(stamp + "  " + text)
        print(stamp + "  " + text)

    def write(self, name, text):
        if self.mode == "dry-run":          # 干跑零产物: 只打印, 不落盘(AC-9 探针面)
            self.log("dry-run: 不落盘 " + name)
            return "(dry-run 未写)"
        os.makedirs(self.evdir, exist_ok=True)
        path = os.path.join(self.evdir, name)
        with open(path, "w", encoding="utf-8", newline=NL) as f:
            f.write(text)
        return path

    # ---------- ① 端口枚举 ----------
    def read_lib_version(self):
        cfg = os.path.join(self.repo, "et_config.h")
        txt = open(cfg, encoding="utf-8", errors="replace").read()
        ma = re.search(r"ET_VERSION_MAJOR\s+([0-9]+)", txt)
        mi = re.search(r"ET_VERSION_MINOR\s+([0-9]+)", txt)
        pa = re.search(r"ET_VERSION_PATCH\s+([0-9]+)", txt)
        if not (ma and mi and pa):
            raise SystemExit("board_window: 无法从 et_config.h 解析版本三分量")
        return "%s.%s.%s" % (ma.group(1), mi.group(1), pa.group(1))

    def enumerate_ports(self):
        try:
            import serial.tools.list_ports as lp
        except ImportError:
            self.abort_reason = "pyserial 未安装（serial.tools.list_ports 不可导入）"
            return []
        found = []
        for p in sorted(lp.comports(), key=lambda x: x.device):
            d = p.description or ""
            found.append((p.device, d, p.hwid or ""))
            self.log("port %s | %s | %s" % (p.device, d, p.hwid or ""))
        hits = [f for f in found if self.desc.lower() in f[1].lower()]
        if len(hits) != 1:
            self.abort_reason = ("按描述 %r 匹配到 %d 个口（须恰好 1 个）；"
                                 "COM 口以枚举实测为准，禁凭记录直连" % (self.desc, len(hits)))
            return []
        self.port = hits[0][0]
        self.log("匹配端口 = %s（描述含 %s）" % (self.port, self.desc))
        return hits

    # ---------- ② 同步 ----------
    def tree_pairs(self):
        pairs = []
        for d in SYNC_DIRS:
            src = os.path.join(self.repo, d)
            dst = os.path.join(self.board, "Core", "et", d)
            for root, _dirs, files in os.walk(src):
                for fn in files:
                    if not fn.endswith((".h", ".c")):
                        continue
                    a = os.path.join(root, fn)
                    b = os.path.join(dst, os.path.relpath(a, src))
                    pairs.append((a, b))
        pairs.append((os.path.join(self.repo, "et_config.h"),
                      os.path.join(self.board, "Core", "et", "et_config.h")))
        pairs.append((os.path.join(self.repo, "port", "port.h"),
                      os.path.join(self.board, "Core", "et", "port", "port.h")))
        return pairs

    def same_file(self, a, b):
        return (os.path.isfile(a) and os.path.isfile(b)
                and sha256_file(a) == sha256_file(b))

    def sync(self):
        """同步面比对与搬运。判定口径（v2.28-r2 T-2, `CO-1(v2.28)`）：
        **abort 只看拷后残差与拷贝失败**；拷前差异 = 同步工作量（首次同步必为非零），
        不得充当判据——旧形态把拷前差异计入 abort 条件，使"板侧落后一个版本"的正常
        首次同步必然中止，而同一份 sync_report 的逐行结论全为 OK。
        """
        lines = ["# board_window 同步面比对（等价 §16.2 step 0 的 diff -rq）",
                 "# 生成时点: " + now(),
                 "# 库侧 HEAD: " + self.git_short_head(),
                 "# 面 = 七目录(*.h/*.c) + et_config.h + port/port.h；et_port/、et_demo.c 工程私有不动",
                 "# 判定 = 拷后残差 + 拷贝失败；拷前差异只作工作量（v2.28-r2 T-2, CO-1）",
                 ""]
        root_et = os.path.join(self.board, "Core", "et")
        pairs = [(s, d) for s, d in self.tree_pairs() if os.path.isfile(s)]

        def rel(path):
            return os.path.relpath(path, root_et).replace(os.sep, "/")

        pre_diff = 0
        for src, dst in pairs:
            if not self.same_file(src, dst):
                pre_diff += 1
                lines.append("DIFF(拷前) %s" % rel(dst))

        copied = 0
        copy_failed = 0
        if self.mode != "dry-run":
            for src, dst in pairs:
                if self.same_file(src, dst):
                    continue
                try:
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    with open(src, "rb") as f:
                        data = f.read()
                    with open(dst, "wb") as f:
                        f.write(data)
                    copied += 1
                except (OSError, IOError) as exc:
                    copy_failed += 1
                    lines.append("COPY-FAIL %s (%s)" % (rel(dst), exc))

        residual = 0
        if self.mode != "dry-run":
            for src, dst in pairs:
                if not self.same_file(src, dst):
                    residual += 1
                    lines.append("RESIDUAL(拷后仍不一致) %s" % rel(dst))

        per_dir = []
        for d in SYNC_DIRS:
            src = os.path.join(self.repo, d)
            dst = os.path.join(root_et, d)
            bad = 0
            for root, _dirs, files in os.walk(src):
                for fn in files:
                    if not fn.endswith((".h", ".c")):
                        continue
                    if not self.same_file(os.path.join(root, fn),
                                          os.path.join(dst, os.path.relpath(os.path.join(root, fn), src))):
                        bad += 1
            per_dir.append("%-10s %s" % (d, "OK (逐文件字节一致)" if bad == 0
                                         else "DIFF x%d (拷后残差)" % bad))
        lines += per_dir
        cfg_b = os.path.join(root_et, "et_config.h")
        cfg_ok = self.same_file(os.path.join(self.repo, "et_config.h"), cfg_b)
        lines.append("et_config.h %s" % ("OK" if cfg_ok else "DIFF"))
        lines.append("板侧 et_config.h MINOR = " + self.board_minor() if os.path.isfile(cfg_b)
                     else "板侧 et_config.h 不存在")
        if self.mode == "dry-run":
            self.log("同步比对(只读): 拷前差异=%d —— dry-run 不写入、不判残差、不中止" % pre_diff)
            self.log("dry-run: 同步面差异清单仅打印，不落盘不写入")
        else:
            self.log("同步比对: 拷前差异=%d / 已拷=%d / 残差=%d / 拷贝失败=%d"
                     % (pre_diff, copied, residual, copy_failed))
            if residual or copy_failed:
                self.abort_reason = ("拷后残差 %d 处、拷贝失败 %d 处 => 同步面未闭合"
                                     "（应逐条排查，不得凭记录断言）" % (residual, copy_failed))
            self.write("sync_report.txt", NL.join(lines) + NL)
        return residual

    def board_minor(self):
        txt = open(os.path.join(self.board, "Core", "et", "et_config.h"),
                   encoding="utf-8", errors="replace").read()
        m = re.search(r"ET_VERSION_MINOR\s+([0-9]+)", txt)
        return m.group(1) if m else "?"

    def git_short_head(self):
        try:
            out = subprocess.check_output(["git", "-C", self.repo, "rev-parse", "--short", "HEAD"],
                                          text=True).strip()
            br = subprocess.check_output(["git", "-C", self.repo, "branch", "--show-current"],
                                         text=True).strip()
            return out + " 分支 " + br
        except Exception:
            return "未知（git 不可用）"

    # ---------- ③ 构建 ----------
    def build(self):
        cmd = ["cmake", "--build", "--preset", self.preset]
        self.log("构建: " + " ".join(cmd))
        p = subprocess.run(cmd, cwd=self.board, capture_output=True, text=True, timeout=1800)
        out = (p.stdout or "") + (p.stderr or "")
        warns = out.count("warning:")
        flash = re.search(r"FLASH:\s+([0-9]+) B", out)
        ram = re.search(r"RAM:\s+([0-9]+) B", out)
        self.write("build.log", out)
        self.log("构建 exit=%d  warning 计数=%d  FLASH=%s  RAM=%s"
                 % (p.returncode, warns,
                    flash.group(1) if flash else "?", ram.group(1) if ram else "?"))
        if flash is None:
            # 增量构建不重链接 => 日志无 MEMORY USAGE 段; 尺寸改由 size 工具直读 ELF
            sz = self.size_of_elf()
            if sz:
                self.log("尺寸(size 工具直读 ELF): text=%d data=%d bss=%d" % sz)
        if p.returncode != 0:
            self.abort_reason = "构建失败（exit=%d）" % p.returncode
        return p.returncode, warns

    def size_of_elf(self):
        tool = self.size_cli
        if not tool or not os.path.isfile(tool):
            self.log("尺寸直读跳过: size 工具不可用（--size-cli 未指向存在的文件）")
            return None
        try:
            out = subprocess.check_output([tool, self.elf_path()], text=True, timeout=60)
        except Exception as exc:
            self.log("尺寸直读失败: %s" % exc)
            return None
        # 输出形态: 制表符分隔 + CRLF, 列 = text data bss dec hex filename
        m = None
        for ln in out.splitlines():
            f = ln.split()
            if len(f) >= 6 and f[0] != "text":
                try:
                    m = (int(f[0]), int(f[1]), int(f[2]))
                except ValueError:
                    m = None
        if m is None:
            self.log("尺寸直读: 输出无法解析: " + out.replace(chr(10), " | "))
            return None
        return m

    # ---------- ④ 烧前版本串机检 ----------
    def elf_path(self):
        return os.path.join(self.board, "build", self.preset, "G474VET6_ET_TEST.elf")

    def version_check(self):
        elf = self.elf_path()
        lines = ["# board_window 烧前 ELF 版本串机检（HC-10② / §16.2）",
                 "# 时点: " + now(),
                 "ELF: " + elf]
        if not os.path.isfile(elf):
            self.abort_reason = "目标 ELF 不存在: " + elf
            self.write("version_check.txt", NL.join(lines) + NL)
            return False
        data = open(elf, "rb").read()
        cands = sorted(set(x.decode() for x in VER_RE.findall(data)))
        hit = self.lib_ver in cands
        lines += ["ELF 版本串集合: %r" % (cands,),
                  "预期板上报串 = %r  命中 = %s" % (self.lib_ver, hit),
                  "干扰串说明: 集合中非预期串多为 ARM 工具链 / HAL / newlib 版本串，"
                  "沿 §16.2 / v2.27 §17 口径逐条排除，不得凭记录断言",
                  "sha256(ELF) = " + hashlib.sha256(data).hexdigest(),
                  "断言: assert %r in cands => %s" % (self.lib_ver, "PASS" if hit else "FAIL")]
        self.write("version_check.txt", NL.join(lines) + NL)
        self.log("机检: 集合=%r 命中=%s" % (cands, hit))
        if not hit:
            self.abort_reason = ("烧前版本串机检不过（ELF 内无 %s）⇒ 按 HC-10② 不烧录"
                                 % self.lib_ver)
            return False
        return True

    # ---------- ⑤ 烧录 ----------
    def flash(self):
        elf = self.elf_path()
        cmd = [self.cli, "-c", "port=SWD", "-w", elf, "-v", "-rst"]
        self.log("烧录: " + " ".join(cmd))
        # 该 CLI 在管道捕获下不回显任何输出(实测 stdout/stderr 皆空但 rc=0)，
        # 故直接落盘成文件再读回: 判据面必须是**原文**，不能只认退出码(HC-8①)。
        os.makedirs(self.evdir, exist_ok=True)
        logpath = os.path.join(self.evdir, "flash.log")
        with open(logpath, "w", encoding="utf-8", errors="replace", newline=NL) as fh:
            p = subprocess.run(cmd, stdout=fh, stderr=subprocess.STDOUT, timeout=600)
        out = open(logpath, encoding="utf-8", errors="replace").read()
        ok = ("Download verified successfully" in out) and (p.returncode == 0)
        self.log("烧录 exit=%d  verified=%s" % (p.returncode, "Download verified successfully" in out))
        if not ok:
            self.abort_reason = "烧录未通过 verify（不得以退出码代替读数判据）"
        return ok

    # ---------- ⑤ 走单 ----------
    def walk(self):
        import serial
        from modbus_master import LoopbackSlave, extract_request   # 复用库内实现
        slave = LoopbackSlave(addr=MODBUS_ADDR, n_regs=16)
        tr = ["# board_window 走单原始全文  " + now(),
              "# 端口 = %s@%d 8N1（描述匹配得到，非凭记录）；从站仿真 addr=0x%02x"
              % (self.port, self.baud, MODBUS_ADDR),
              "# 模式 = %s；库侧预期版本串 = %s" % (self.mode, self.lib_ver),
              ""]

        def ts():
            return time.strftime("%H:%M:%S")

        def serve(buf):
            got = extract_request(bytes(buf), MODBUS_ADDR)
            if not got:
                return 0
            frame, end = got
            rep = slave.handle(frame)
            tr.append("%s  [slave] req = %s" % (ts(), " ".join("%02x" % b for b in frame)))
            if rep:
                ser.write(bytes(rep))
                ser.flush()
                tr.append("%s  [slave] resp = %s" % (ts(), " ".join("%02x" % b for b in rep)))
            return end

        def collect(seconds, label):
            buf, text = bytearray(), bytearray()
            t_end = time.time() + seconds
            idle = t_end - 0.25
            while time.time() < t_end:
                n = ser.in_waiting
                data = ser.read(n if n else 1)
                if data:
                    buf += data
                    idle = t_end
                    while True:
                        used = serve(buf)
                        if used == 0:
                            break
                        del buf[:used]
                    cut = len(buf)
                    for i, b in enumerate(buf):
                        if b in (MODBUS_ADDR, 0x00):
                            cut = i
                            break
                    if cut:
                        text += buf[:cut]
                        del buf[:cut]
                else:
                    time.sleep(0.005)
                    if time.time() > idle:
                        break
            if buf:
                text += buf
            if text:
                tr.append("%s  [%s] %s" % (ts(), label,
                                           bytes(text).decode("utf-8", "replace").strip()))
            return bytes(text)

        def send(raw, label, wait=1.5):
            tr.append("%s  >>> %s (%s)" % (ts(), label, " ".join("%02x" % b for b in raw)))
            ser.write(raw)
            ser.flush()
            return collect(wait, label)

        ser = serial.Serial(self.port, self.baud, timeout=0.02)
        ser.reset_input_buffer()
        send(CR, "WAKE", 0.8)
        for cmd, label, wait in (
            (b"AT+VER", "AT+VER", 1.0),
            (b"AT+BOOTINFO", "AT+BOOTINFO", 1.5),
            (b"AT+FEATURES", "AT+FEATURES", 2.5),
            (b"AT+SELFTEST", "AT+SELFTEST", 6.0),
            (b"AT+MBRD 0 4" + CRLF, "MBRD-F1-CRLF", 3.0),
            (b"AT+MBSTAT" + CRLF, "MBSTAT-F1", 1.0),
            (b"AT+MBRD 0 4" + CR, "MBRD-F2-CR", 3.0),
            (b"AT+MBSTAT" + CRLF, "MBSTAT-F2", 1.0),
            (b"AT+MBRD 0 4" + LF, "MBRD-F3-LF", 3.0),
            (b"AT+MBSTAT" + CRLF, "MBSTAT-F3", 1.0),
            (b"AT+METRICS", "AT+METRICS", 2.0),
            (b"AT+RXSTAT", "AT+RXSTAT", 1.0),
        ):
            raw = cmd + (CRLF if label.startswith("AT+") else b"")
            send(raw, label, wait)
        ser.close()
        path = self.write("walk_output.txt", NL.join(tr) + NL)
        self.log("走单落盘 %s（%d 行）; 从站 served=%d crc_err=%d mismatch=%d"
                 % (path, len(tr), slave.served, slave.crc_err, slave.mismatch))
        self.summarise(tr, slave)

    def summarise(self, tr, slave):
        joined = NL.join(tr)
        rows = [
            ("AT+VER", r"ver=[0-9.]+ boot=[0-9]+", "板上版本串须 = 库侧 %s" % self.lib_ver),
            ("AT+BOOTINFO", r"staged=. confirmed=. attempts=[0-9]+", "HC-10④ 三字段在案"),
            ("AT+FEATURES", r"FEATURES v[0-9.]+ \(maj=[0-9]+ min=[0-9]+ pat=[0-9]+\) sw=[0-9]+", "自描述表头"),
            ("AT+SELFTEST", r"SELFTEST: [0-9]+/[0-9]+ PASS", "板上自测"),
            ("MBRD 三形态", r"disc=0", "三行 disc=0 须齐（CRLF / 仅 CR / 仅 LF）"),
            ("AT+METRICS", r"METRICS [0-9]+/[0-9]+ drop=[0-9]+", "命名指标 dump"),
            ("AT+RXSTAT", r"rx=[0-9]+ ore=[0-9]+", "UART 健康"),
        ]
        out = ["# board_window 走单逐项摘要（原始全文 = walk_output.txt）",
               "# 生成时点: " + now() + "  模式: " + self.mode,
               "",
               "| 走单项 | 命中读数 | 判据 |",
               "|---|---|---|"]
        for label, pat, note in rows:
            m = re.findall(pat, joined)
            out.append("| %s | %s | %s |" % (label, (m[0] if m else "未命中"), note))
        out.append("")
        out.append("PC 侧从站仿真计数: served=%d crc_err=%d mismatch=%d"
                   % (slave.served, slave.crc_err, slave.mismatch))
        out.append("")
        out.append("注意（HC-8①）：本摘要由上位机侧从**串口原始文本**提取；脚本退出码 0 "
                   "只代表机检通过，板侧绿的判据是本表与 walk_output.txt 的原文行。")
        self.write("walk_summary.md", NL.join(out) + NL)

    # ---------- 编排 ----------
    def plan(self):
        self.log("计划面（本次模式 = %s）:" % self.mode)
        self.log("  库侧版本串预期      = " + self.lib_ver)
        self.log("  证据目录            = " + self.evdir)
        self.log("  板侧工程 / CLI      = %s / %s" % (self.board, self.cli))
        self.log("  同步面              = %s + et_config.h + port/port.h" % ", ".join(SYNC_DIRS))
        self.log("  构建 preset         = " + self.preset)
        self.log("  是否烧录 / 是否走单 = %s / %s"
                 % (self.mode == "full", self.mode == "full"))

    def run(self):
        if self.mode != "dry-run":
            os.makedirs(self.evdir, exist_ok=True)
        self.log("board_window 启动  " + now() + "  模式=" + self.mode)
        self.plan()
        if not self.enumerate_ports():
            return self.finish()
        if self.mode == "dry-run":
            self.sync()                      # 只比对不写入
            self.version_check()             # 对现存 ELF 做机检（不构建不烧录）
        else:
            self.sync()
            if self.abort_reason is None:
                self.build()
            if self.abort_reason is None:
                if not self.version_check():
                    return self.finish()     # 机检红 => 绝不烧录
            if self.mode == "full" and self.abort_reason is None:
                if self.flash():
                    sys.path.insert(0, os.path.join(self.repo, "tools"))
                    self.walk()
        return self.finish()

    def finish(self):
        head = ["# board_window 运行摘要（五要素留痕入口）",
                "# 时点: " + now(),
                "# 模式: %s   库侧版本串: %s   端口: %s" % (self.mode, self.lib_ver,
                                                        getattr(self, "port", "未定")),
                "# 中止原因: " + (self.abort_reason or "无（五要素上位机侧机检均通过）"),
                "# 再次声明: 退出码 0 = 上位机侧机检通过，**不等于板侧绿**（读数见 walk_*）",
                "",
                NL.join(self.lines),
                ""]
        self.write("window_run.log", NL.join(head) + NL)
        if self.abort_reason:
            print("board_window: 中止 => " + self.abort_reason)
            return 1
        print("board_window: 机检通过（模式 %s）。板侧判据请核 walk_output.txt / walk_summary.md。"
              % self.mode)
        return 0


def main():
    ap = argparse.ArgumentParser(description="G474 板侧窗口一键化（v2.28 REQ-5 / M3-⑤）")
    ap.add_argument("--mode", choices=("dry-run", "check-only", "full"), default="dry-run")
    ap.add_argument("--repo", default=REPO_DEFAULT)
    ap.add_argument("--board", default=BOARD_DEFAULT)
    ap.add_argument("--cli", default=CLI_DEFAULT)
    ap.add_argument("--desc", default=PORT_DESC_DEFAULT, help="按串口设备描述匹配端口")
    ap.add_argument("--baud", type=int, default=BAUD_DEFAULT)
    ap.add_argument("--preset", default="Debug")
    ap.add_argument("--evidence", default=None, help="证据目录名，默认 v<主.次>-evidence")
    ap.add_argument("--size-cli", default=SIZE_DEFAULT,
                    help="arm-none-eabi-size 路径，用于增量构建时直读 ELF 尺寸")
    a = ap.parse_args()
    return Window(a).run()


if __name__ == "__main__":
    sys.exit(main())
