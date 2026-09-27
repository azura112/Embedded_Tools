# build.py - command line build for the et-refactored CH32X035G8U PD firmware
# located in the Embedded_Tools repo (port/ch32x035/).
#
# Mirrors the MounRiver Studio 2 (.cproject) flags of the original CH32X035G8U
# project and adds the Embedded_Tools sources + include paths. Vendor code
# (Peripheral/Core/Debug) keeps the original light warning set; the et sources,
# port layer and app build with -Wall -Wextra.
#
# Usage: python build.py
import pathlib, subprocess, os, sys, shutil

PROJ = pathlib.Path(__file__).resolve().parent
ET_ROOT = PROJ.parent.parent           # port/ch32x035 -> port -> Embedded_Tools
GCC_DIR = r"D:\software\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin"
GCC = str(pathlib.Path(GCC_DIR) / "riscv-none-embed-gcc.exe")
OBJCOPY = str(pathlib.Path(GCC_DIR) / "riscv-none-embed-objcopy.exe")
SIZE = str(pathlib.Path(GCC_DIR) / "riscv-none-embed-size.exe")

if not os.path.exists(GCC):
    sys.exit("toolchain not found: " + GCC)

env = dict(os.environ)
env["PATH"] = GCC_DIR + ";" + env.get("PATH", "")

COMMON = [
    "-march=rv32imacxw", "-mabi=ilp32", "-msmall-data-limit=0", "-msave-restore",
    "-Os", "-g", "-std=gnu99",
    "-fmessage-length=0", "-fsigned-char", "-ffunction-sections", "-fdata-sections",
    "-fno-common", "-Wunused", "-Wuninitialized",
]

# Embedded_Tools 裁剪: 本固件只启用 ringbuf/queue/sched/wdt/key/led/log;
# KV 启用: 参数区 = 片内 62K flash 尾部 2×1KB(Link.ld 代码区 60K 预留);
# BOOTCTL 未启用。CDC 环容量均为 2 的幂。
DEFS = [
    "-DET_RINGBUF_POW2=1",
    "-DET_MODULE_KV=1",
    "-DPORT_FLASH_SECTOR_COUNT=2",
    "-DET_MODULE_BOOTCTL=0",
]

INCLUDES = [
    "-IStartup", "-IDebug", "-ICore", "-Iapp", "-IPeripheral/inc",
    "-I.",                       # port_ch32x035.h
    "-I" + str(ET_ROOT),         # et_config.h
    "-I" + str(ET_ROOT / "port"),       # port.h
    "-I" + str(ET_ROOT / "core"),
    "-I" + str(ET_ROOT / "sys"),
    "-I" + str(ET_ROOT / "drivers"),
    "-I" + str(ET_ROOT / "debug"),
    "-I" + str(ET_ROOT / "protocol"),
    "-I" + str(ET_ROOT / "storage"),
]

STRICT = ["-Wall", "-Wextra"]    # et 源 + port + app 零警告门

OUTDIR = PROJ / "build"
if OUTDIR.exists():
    shutil.rmtree(OUTDIR)
OUTDIR.mkdir()

def run(args):
    r = subprocess.run([str(a) for a in args], env=env)
    if r.returncode != 0:
        sys.exit("command failed: " + " ".join(str(a) for a in args))

def rel(p):
    rp = pathlib.Path(p).resolve()
    try:
        return str(rp.relative_to(PROJ))
    except ValueError:                       # ET 源码位于本工程目录之外
        return str(rp)

objs = []

def cc(src, extra):
    obj = OUTDIR / (pathlib.Path(src).stem + "_" + str(len(objs)) + ".o")
    print("CC  " + rel(src))
    run([GCC] + COMMON + DEFS + INCLUDES + extra + ["-c", str(src), "-o", str(obj)])
    objs.append(obj)

# vendor 层: 沿用原工程警告集
for d in ["Peripheral/src", "Core", "Debug"]:
    for src in sorted((PROJ / d).glob("*.c")):
        cc(src, [])

# 应用 + port 层: -Wall -Wextra
for src in sorted((PROJ / "app").glob("*.c")):
    cc(src, STRICT)
cc(PROJ / "port_ch32x035.c", STRICT)

# Embedded_Tools 模块(仅本固件启用的七个)
for relmod in ["core/et_ringbuf.c", "core/et_queue.c", "sys/et_sched.c",
               "sys/et_wdt.c", "drivers/et_key.c", "drivers/et_led.c",
               "debug/et_log.c", "protocol/et_crc.c", "storage/et_kv.c"]:
    cc(ET_ROOT / relmod, STRICT)

startup = PROJ / "Startup" / "startup_ch32x035.S"
objS = OUTDIR / "startup_ch32x035.o"
print("AS  Startup/startup_ch32x035.S")
run([GCC] + COMMON + DEFS + INCLUDES +
    ["-x", "assembler-with-cpp", "-c", str(startup), "-o", str(objS)])
objs.insert(0, objS)

elf = OUTDIR / "CH32X035G8U.elf"
print("LINK " + elf.name)
run([GCC] + COMMON + [
    "-nostartfiles",
    "-Xlinker", "--gc-sections",
    "-Wl,--gc-sections",
    "-TLink.ld",
    "--specs=nano.specs",
    "--specs=nosys.specs",
] + [str(o) for o in objs] + ["-o", str(elf)])

run([OBJCOPY, "-O", "ihex", str(elf), str(OUTDIR / "CH32X035G8U.hex")])
run([OBJCOPY, "-O", "binary", str(elf), str(OUTDIR / "CH32X035G8U.bin")])
run([SIZE, str(elf)])
print("BUILD OK")
