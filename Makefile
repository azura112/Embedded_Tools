# Embedded_Tools 宿主机单元测试构建
CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -pedantic -I. -Icore -Isys -Iprotocol -Idrivers -Idebug -Iport -Iport/host
OBJDIR  := build

CORE_SRC    := core/et_ringbuf.c core/et_queue.c core/et_mempool.c
SYS_SRC     := sys/et_stimer.c sys/et_sched.c sys/et_event.c
PROTO_SRC   := protocol/et_crc.c protocol/et_frame.c protocol/et_atcmd.c
DRIVERS_SRC := drivers/et_key.c drivers/et_led.c
DEBUG_SRC   := debug/et_log.c debug/et_assert.c
PORT_SRC    := port/host/port_host.c
LIB_SRC     := $(CORE_SRC) $(SYS_SRC) $(PROTO_SRC) $(DRIVERS_SRC) $(DEBUG_SRC)

TEST_SRC := test/et_test.c test/test_ringbuf.c test/test_queue.c test/test_mempool.c \
            test/test_stimer.c test/test_sched.c test/test_event.c \
            test/test_crc.c test/test_frame.c test/test_atcmd.c \
            test/test_key.c test/test_led.c \
            test/test_log.c test/test_assert.c test/test_main.c

DEMO_SRC := examples/posix_demo.c

.PHONY: all test demo clean

all: test

test: $(OBJDIR)/et_tests.exe
	./$(OBJDIR)/et_tests.exe

$(OBJDIR)/et_tests.exe: $(LIB_SRC) $(PORT_SRC) $(TEST_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $(LIB_SRC) $(PORT_SRC) $(TEST_SRC)

demo: $(OBJDIR)/demo.exe
	./$(OBJDIR)/demo.exe

$(OBJDIR)/demo.exe: $(LIB_SRC) $(PORT_SRC) $(DEMO_SRC)
	-mkdir $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $(LIB_SRC) $(PORT_SRC) $(DEMO_SRC)

clean:
	-rm -rf $(OBJDIR)
