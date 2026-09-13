#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""modbus_master.py —— Modbus RTU 主站工具 (v2.4, 从站真机走单)

对端: Embedded_Tools et_modbus 从站 (protocol/et_modbus.h) —— 功能码
0x03/0x04 读、0x06/0x10 写; 异常 0x01/0x02/0x03; CRC16-MODBUS **低字节在前**。

用法:
  回环自测 (不接串口, 内置从站仿真器逐条验证本工具的编解码/CRC/异常路径):
    python tools/modbus_master.py --selftest

  真机读寄存器 (板上从站 addr=17):
    python tools/modbus_master.py --port COM12 --read 0 --qty 4
    python tools/modbus_master.py --port COM12 --read 0 --qty 4 --func 4

  真机写寄存器:
    python tools/modbus_master.py --port COM12 --write 2=4660
    python tools/modbus_master.py --port COM12 --write-multi 1=17,34,51

  发任意字节 (CRC 坏帧静默验证等):
    python tools/modbus_master.py --port COM12 --raw 11030000000100FF

返回码: 0 = 成功; 1 = 协议/超时失败; 2 = 参数或自检失败。
"""
import argparse
import sys
import time

# ---- 功能码 / 异常码 ----
FC_RD_HOLDING = 0x03
FC_RD_INPUT = 0x04
FC_WR_SINGLE = 0x06
FC_WR_MULTIPLE = 0x10

EXC_NAME = {
    0x01: "ILLEGAL FUNCTION",
    0x02: "ILLEGAL DATA ADDRESS",
    0x03: "ILLEGAL DATA VALUE",
}


def crc16_modbus(data):
    """CRC16-MODBUS: poly 0xA001(反射), init 0xFFFF —— 与 et_crc16_modbus 一致"""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def build_frame(addr, func, payload):
    """组帧: [addr, func, payload..., crc_lo, crc_hi(低字节在前)]"""
    body = bytes([addr, func]) + bytes(payload)
    crc = crc16_modbus(body)
    return body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def check_crc(frame):
    if len(frame) < 4:
        return False
    wire = frame[-2] | (frame[-1] << 8)
    return wire == crc16_modbus(frame[:-2])


def parse_response(frame, addr, func):
    """解析应答: 返回 (kind, value); kind ∈ {'read','write','exc'}"""
    if not check_crc(frame):
        raise ValueError("应答 CRC 校验失败: " + frame.hex(" ").upper())
    if frame[0] != addr:
        raise ValueError("应答地址不符: 0x%02X" % frame[0])
    if frame[1] & 0x80:
        return ("exc", frame[2])
    if frame[1] != func:
        raise ValueError("应答功能码不符: 0x%02X" % frame[1])
    if func in (FC_RD_HOLDING, FC_RD_INPUT):
        bc = frame[2]
        data = frame[3:3 + bc]
        if len(data) != bc or (bc % 2) != 0:
            raise ValueError("应答字节数异常: %d" % bc)
        return ("read", [(data[i] << 8) | data[i + 1] for i in range(0, bc, 2)])
    return ("write", None)


# ===================== 内置从站仿真器 (--selftest 用) =====================
class LoopbackSlave:
    """最小从站: 与 et_modbus 语义对齐(供本工具自测, 非库实现)"""

    def __init__(self, addr=0x11, n_regs=16):
        self.addr = addr
        self.hold = [i + 1 for i in range(n_regs)]
        self.n = n_regs
        self.crc_err = 0
        self.mismatch = 0

    def handle(self, frame):
        if not check_crc(frame):
            self.crc_err += 1
            return None
        if frame[0] not in (0, self.addr):
            self.mismatch += 1
            return None
        if frame[0] == 0:                       # 广播: 执行写, 不应答
            self._exec(frame, reply=False)
            return None
        return self._exec(frame, reply=True)

    def _exec(self, frame, reply):
        func = frame[1]
        if func in (FC_RD_HOLDING, FC_RD_INPUT):
            start = (frame[2] << 8) | frame[3]
            qty = (frame[4] << 8) | frame[5]
            if qty == 0 or qty > 125:
                return self._exc(func, 0x03)
            if start + qty > self.n:
                return self._exc(func, 0x02)
            data = []
            for i in range(qty):
                data += [(self.hold[start + i] >> 8) & 0xFF, self.hold[start + i] & 0xFF]
            return build_frame(self.addr, func, [2 * qty] + data) if reply else None
        if func == FC_WR_SINGLE:
            start = (frame[2] << 8) | frame[3]
            if start >= self.n:
                return self._exc(func, 0x02) if reply else None
            self.hold[start] = (frame[4] << 8) | frame[5]
            return build_frame(self.addr, func, list(frame[2:6])) if reply else None
        if func == FC_WR_MULTIPLE:
            start = (frame[2] << 8) | frame[3]
            qty = (frame[4] << 8) | frame[5]
            bc = frame[6]
            if qty == 0 or qty > 123 or bc != 2 * qty:
                return self._exc(func, 0x03) if reply else None
            if start + qty > self.n:
                return self._exc(func, 0x02) if reply else None
            for i in range(qty):
                self.hold[start + i] = (frame[7 + 2 * i] << 8) | frame[8 + 2 * i]
            return build_frame(self.addr, func, list(frame[2:6])) if reply else None
        return self._exc(func, 0x01) if reply else None

    def _exc(self, func, code):
        return build_frame(self.addr, func | 0x80, [code])


def selftest():
    """回环自测: CRC 标准向量 + 编解码 + 异常路径 + 静默语义"""
    fails = 0

    def chk(cond, name):
        nonlocal fails
        print("  %-34s %s" % (name, "ok" if cond else "FAIL"))
        if not cond:
            fails += 1

    # 1. CRC 标准向量: "123456789" → 0x4B37
    chk(crc16_modbus(b"123456789") == 0x4B37, "CRC vector 123456789 = 0x4B37")
    # 2. 组帧字节序: 0x03 读 0..3 → 11 03 00 00 00 04 44 09 (低字节在前)
    f = build_frame(0x11, FC_RD_HOLDING, [0x00, 0x00, 0x00, 0x04])
    chk(f.hex().upper() == "1103000000044699", "frame bytes (CRC low first)")
    chk(check_crc(f), "self CRC accepted")
    f2 = bytearray(f); f2[-1] ^= 0xFF
    chk(not check_crc(bytes(f2)), "corrupted CRC rejected")

    # 3. 从站回环: 读/写/异常/广播/静默
    sl = LoopbackSlave(0x11)
    r = sl.handle(build_frame(0x11, FC_RD_HOLDING, [0, 0, 0, 2]))
    chk(parse_response(r, 0x11, FC_RD_HOLDING) == ("read", [1, 2]), "read holding 0..1")
    r = sl.handle(build_frame(0x11, FC_WR_SINGLE, [0, 2, 0x12, 0x34]))
    chk(sl.hold[2] == 0x1234, "write single applied")
    chk(parse_response(r, 0x11, FC_WR_SINGLE)[0] == "write", "write single echo")
    r = sl.handle(build_frame(0x11, FC_WR_MULTIPLE, [0, 1, 0, 2, 4, 0, 7, 0, 9]))
    chk(sl.hold[1] == 7 and sl.hold[2] == 9, "write multiple applied")
    r = sl.handle(build_frame(0x11, FC_RD_HOLDING, [0, 0, 0, 0]))
    chk(parse_response(r, 0x11, FC_RD_HOLDING) == ("exc", 0x03), "qty=0 -> exc 0x03")
    r = sl.handle(build_frame(0x11, FC_RD_HOLDING, [0, 15, 0, 4]))
    chk(parse_response(r, 0x11, FC_RD_HOLDING) == ("exc", 0x02), "out of range -> exc 0x02")
    r = sl.handle(build_frame(0x11, 0x63, []))
    chk(parse_response(r, 0x11, 0x63) == ("exc", 0x01), "unknown func -> exc 0x01")
    chk(sl.handle(build_frame(0, FC_WR_SINGLE, [0, 3, 0, 0x21])) is None,
        "broadcast write: no reply")
    chk(sl.hold[3] == 0x21, "broadcast write applied")
    bad = bytearray(build_frame(0x11, FC_RD_HOLDING, [0, 0, 0, 1])); bad[-2] ^= 0x55
    chk(sl.handle(bytes(bad)) is None and sl.crc_err == 1, "bad CRC silent")
    chk(sl.handle(build_frame(0x22, FC_RD_HOLDING, [0, 0, 0, 1])) is None
        and sl.mismatch == 1, "addr mismatch silent")

    print("[modbus_master --selftest] %s" % ("PASS" if fails == 0 else "FAIL"))
    return 0 if fails == 0 else 2


# ===================== 串口模式 =====================
def extract_frame(buf, addr):
    """从接收缓冲提取一个 CRC 有效的完整应答帧(抗日志/心跳混入), 无则 None"""
    n = len(buf)
    for i in range(n):
        if addr != 0 and buf[i] != addr:
            continue
        f = buf[i:]
        if len(f) < 5:
            continue
        fc = f[1]
        if fc & 0x80:
            cand = f[:5]
        elif fc in (FC_RD_HOLDING, FC_RD_INPUT):
            need = 3 + f[2] + 2
            if len(f) < need:
                continue
            cand = f[:need]
        elif fc in (FC_WR_SINGLE, FC_WR_MULTIPLE):
            if len(f) < 8:
                continue
            cand = f[:8]
        else:
            continue
        if check_crc(cand):
            return cand
    return None


def transact(ser, tx, addr, func, timeout, verbose):
    if verbose:
        print("  tx: %s" % tx.hex(" ").upper())
    ser.reset_input_buffer()
    ser.write(tx)
    ser.flush()
    deadline = time.time() + timeout
    buf = bytearray()
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        buf += b
        f = extract_frame(bytes(buf), addr)      # 只认 CRC 有效帧(抗日志混入)
        if f is not None:
            if verbose:
                print("  rx: %s" % f.hex(" ").upper())
            return f
    if verbose and buf:
        print("  rx(raw): %s" % bytes(buf).hex(" ").upper())
    print("  rx: <超时无应答>")
    return None


def main():
    ap = argparse.ArgumentParser(description="Modbus RTU slave walkthrough tool "
                                             "(paired with et_modbus)")
    ap.add_argument("--selftest", action="store_true", help="回环自测(不接串口)")
    ap.add_argument("--port", help="串口名 (如 COM12 / /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--addr", type=lambda s: int(s, 0), default=0x11,
                    help="从站地址 (默认 0x11=17)")
    ap.add_argument("--read", type=lambda s: int(s, 0), help="读起始寄存器")
    ap.add_argument("--qty", type=int, default=1, help="读寄存器数 (默认 1)")
    ap.add_argument("--func", type=lambda s: int(s, 0), default=0x03,
                    help="读功能码 0x03/0x04 (默认 0x03)")
    ap.add_argument("--write", help="单写 ADDR=VAL (如 --write 2=4660)")
    ap.add_argument("--write-multi", help="多写 ADDR=V1,V2,...")
    ap.add_argument("--raw", help="直接发送十六进制字节 (期望静默: CRC 坏/地址不符)")
    ap.add_argument("--raw-resp", help="直接发送十六进制字节 (期望有应答, 校验 CRC)")
    ap.add_argument("--timeout", type=float, default=1.0, help="应答超时秒")
    ap.add_argument("--settle", type=float, default=0.02,
                    help="写后静默 (秒, 让从站收完帧)")
    ap.add_argument("--expect-exc", type=lambda s: int(s, 0), default=None,
                    help="期望收到该异常码(如 0x02): 收到即 PASS, 未收到 FAIL")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if not args.port:
        print("需要 --port (或 --selftest)", file=sys.stderr)
        return 2
    if (args.read is None and not args.write and not args.write_multi
            and not args.raw and not args.raw_resp):
        print("需要 --read / --write / --write-multi / --raw / --raw-resp 之一", file=sys.stderr)
        return 2

    try:
        import serial
    except ImportError:
        print("需要 pyserial: pip install pyserial", file=sys.stderr)
        return 2

    fail = 0
    with serial.Serial(args.port, args.baud, timeout=0.05) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()

        if args.raw:
            tx = bytes.fromhex(args.raw)
            rx = transact(ser, tx, args.addr, 0, args.timeout, args.verbose)
            if rx is None:
                print("[modbus] raw: 静默(无应答) —— 符合预期则 CRC 坏帧/地址不符路径正常")
            else:
                print("[modbus] raw: 意外应答 %s" % rx.hex(" ").upper())
                fail += 1

        if args.raw_resp:
            tx = bytes.fromhex(args.raw_resp)
            rx = transact(ser, tx, args.addr, 0, args.timeout, args.verbose)
            if rx is None:
                print("[modbus] raw-resp: 超时无应答 (期望有应答)")
                fail += 1
            elif not check_crc(rx):
                print("[modbus] raw-resp: 应答 CRC 无效")
                fail += 1
            elif rx[1] & 0x80:
                print("[modbus] raw-resp: 异常 0x%02X %s"
                      % (rx[2], EXC_NAME.get(rx[2], "")))
                fail += 0 if args.expect_exc == rx[2] else 1
            else:
                print("[modbus] raw-resp: 正常应答 %s" % rx.hex(" ").upper())
            time.sleep(args.settle)

        if args.write:
            a, v = args.write.split("=")
            tx = build_frame(args.addr, FC_WR_SINGLE,
                             [int(a, 0) >> 8, int(a, 0) & 0xFF,
                              int(v, 0) >> 8, int(v, 0) & 0xFF])
            rx = transact(ser, tx, args.addr, FC_WR_SINGLE, args.timeout, args.verbose)
            if rx is None:
                print("[modbus] write single: 超时无应答")
                fail += 1
            else:
                kind, val = parse_response(rx, args.addr, FC_WR_SINGLE)
                if kind == "exc":
                    print("[modbus] write single: 异常 0x%02X %s" % (val, EXC_NAME.get(val, "")))
                    fail += 0 if args.expect_exc == val else 1
                else:
                    print("[modbus] write single ok: reg %s = %s" % (a, v))
            time.sleep(args.settle)

        if args.write_multi:
            a, vals = args.write_multi.split("=")
            start = int(a, 0)
            vlist = [int(x, 0) for x in vals.split(",")]
            payload = [start >> 8, start & 0xFF, len(vlist) >> 8, len(vlist) & 0xFF,
                       2 * len(vlist)]
            for v in vlist:
                payload += [v >> 8, v & 0xFF]
            tx = build_frame(args.addr, FC_WR_MULTIPLE, payload)
            rx = transact(ser, tx, args.addr, FC_WR_MULTIPLE, args.timeout, args.verbose)
            if rx is None:
                print("[modbus] write multiple: 超时无应答")
                fail += 1
            else:
                kind, val = parse_response(rx, args.addr, FC_WR_MULTIPLE)
                if kind == "exc":
                    print("[modbus] write multiple: 异常 0x%02X %s" % (val, EXC_NAME.get(val, "")))
                    fail += 0 if args.expect_exc == val else 1
                else:
                    print("[modbus] write multiple ok: reg %d..%d = %s"
                          % (start, start + len(vlist) - 1, vlist))
            time.sleep(args.settle)

        if args.read is not None:
            tx = build_frame(args.addr, args.func,
                             [args.read >> 8, args.read & 0xFF,
                              args.qty >> 8, args.qty & 0xFF])
            rx = transact(ser, tx, args.addr, args.func, args.timeout, args.verbose)
            if rx is None:
                print("[modbus] read: 超时无应答")
                fail += 1
            else:
                kind, val = parse_response(rx, args.addr, args.func)
                if kind == "exc":
                    print("[modbus] read: 异常 0x%02X %s" % (val, EXC_NAME.get(val, "")))
                    fail += 0 if args.expect_exc == val else 1
                else:
                    print("[modbus] read %d..%d (fc=0x%02X): %s"
                          % (args.read, args.read + args.qty - 1, args.func,
                             ", ".join(str(v) for v in val)))

    print("[modbus_master] %s" % ("PASS" if fail == 0 else "FAIL"))
    return 0 if fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
