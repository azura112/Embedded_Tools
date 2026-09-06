# Embedded_Tools 宿主机单元测试构建
#
# ── Windows 无 make 环境快速构建 ─────────────────────────────────────────
# 本库在 Windows 下可按以下任一方式构建(任选其一):
#
#   1) 安装了 MinGW-w64 / MSYS2: 通常自带 make 或 mingw32-make
#        mingw32-make test
#        mingw32-make demo
#
#   2) 纯 gcc + Git Bash(命令可直接复制, 无需任何构建工具):
#        mkdir -p build
#        gcc -std=c99 -Wall -Wextra -pedantic -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/host \
#            -o build/et_tests.exe core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c storage/*.c \
#            port/host/port_host.c test/*.c
#        ./build/et_tests.exe
#
#        gcc -std=c99 -Wall -Wextra -pedantic -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/host \
#            -o build/demo.exe core/*.c algorithm/*.c sys/*.c protocol/*.c drivers/*.c debug/*.c storage/*.c \
#            port/host/port_host.c examples/posix_demo.c
#        ./build/demo.exe
#
#   注: 在 cmd/PowerShell 中把 mkdir -p build 换成 mkdir build, 把 ./ 换成 .\。
# ─────────────────────────────────────────────────────────────────────────
CC      := gcc
# -DET_MODULE_SELFTEST=1: host 侧复用板上自测组件(默认 0, 见 et_config.h); test 与
#       demo 同跑一遍, 板上/PC 结果可比对 (v1.7)
CFLAGS  := -std=c99 -Wall -Wextra -pedantic -DET_MODULE_SELFTEST=1 -I. -Icore -Ialgorithm -Isys -Iprotocol -Idrivers -Idebug -Istorage -Iport -Iport/host
OBJDIR  := build

CORE_SRC    := core/et_ringbuf.c core/et_queue.c core/et_mempool.c core/et_list.c
ALGO_SRC    := algorithm/et_filter.c algorithm/et_fsm.c
SYS_SRC     := sys/et_stimer.c sys/et_sched.c sys/et_event.c sys/et_softclock.c sys/et_wdt.c
PROTO_SRC   := protocol/et_crc.c protocol/et_frame.c protocol/et_atcmd.c protocol/et_xmodem.c
DRIVERS_SRC := drivers/et_key.c drivers/et_led.c drivers/et_spwm.c
DEBUG_SRC   := debug/et_log.c debug/et_assert.c debug/et_shell.c debug/et_selftest.c
STORAGE_SRC := storage/et_kv.c storage/et_bootctl.c
PORT_SRC    := port/host/port_host.c
LIB_SRC     := $(CORE_SRC) $(ALGO_SRC) $(SYS_SRC) $(PROTO_SRC) $(DRIVERS_SRC) $(DEBUG_SRC) $(STORAGE_SRC)

TEST_SRC := test/et_test.c test/test_ringbuf.c test/test_queue.c test/test_mempool.c \
            test/test_list.c test/test_filter.c test/test_fsm.c \
            test/test_stimer.c test/test_sched.c test/test_event.c test/test_softclock.c \
            test/test_crc.c test/test_frame.c test/test_atcmd.c test/test_xmodem.c test/test_shell.c test/test_shell_hist.c \
            test/test_key.c test/test_led.c test/test_spwm.c \
            test/test_log.c test/test_assert.c test/test_kv.c test/test_bootctl.c test/test_wdt.c 
            test/test_selftest.c test/test_main.c

DEMO_SRC := examples/posix_demo.c

# G4 flash 几何变体 (v1.6 双几何回归): storage 布局类改动必须 F1/G4 双几何全绿
G4FLAGS := -DPORT_FLASH_SECTOR_SIZE=2048 -DPORT_FLASH_SECTOR_COUNT=16 -DPORT_FLASH_ERASE_MS_MAX=40

.PHONY: all test demo test-g4 bench bench-table clean

all: test

test: $(OBJDIR)/et_tests.exe
	./$(OBJDIR)/et_tests.exe

$(OBJDIR)/et_tests.exe: $(LIB_SRC) $(PORT_SRC) $(TEST_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $(LIB_SRC) $(PORT_SRC) $(TEST_SRC)

# 双几何变体: G474 2KB 页几何下同一套用例 (与默认 F1 1KB 几何互为回归矩阵)
test-g4: $(OBJDIR)/et_tests_g4.exe
	./$(OBJDIR)/et_tests_g4.exe

$(OBJDIR)/et_tests_g4.exe: $(LIB_SRC) $(PORT_SRC) $(TEST_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(G4FLAGS) -o $@ $(LIB_SRC) $(PORT_SRC) $(TEST_SRC)

# host 基准 (v1.7): 数字入 docs/bench.md 须附环境注记; 查表变体单独构建
bench: $(OBJDIR)/bench.exe
	./$(OBJDIR)/bench.exe

$(OBJDIR)/bench.exe: tools/bench.c $(LIB_SRC) $(PORT_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) -O2 -o $@ tools/bench.c $(LIB_SRC) $(PORT_SRC)

bench-table: $(OBJDIR)/bench_table.exe
	./$(OBJDIR)/bench_table.exe

$(OBJDIR)/bench_table.exe: tools/bench.c $(LIB_SRC) $(PORT_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) -DET_CRC_TABLE=1 -O2 -o $@ tools/bench.c $(LIB_SRC) $(PORT_SRC)

demo: $(OBJDIR)/demo.exe
	./$(OBJDIR)/demo.exe

$(OBJDIR)/demo.exe: $(LIB_SRC) $(PORT_SRC) $(DEMO_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $(LIB_SRC) $(PORT_SRC) $(DEMO_SRC)

clean:
	-rm -rf $(OBJDIR)
