#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""xmodem_send.py —— XMODEM-CRC 发送端 (v1.6 P1, 升级链真机走单工具)

对端: Embedded_Tools et_xmodem 接收器 (NAK 催块 / CRC16-CCITT 高字节在前 /
EOT 两段确认, 见 protocol/et_xmodem.h 头注)。

用法:
  串口发送 (先在板上执行 AT+UPGRADE 进入接收):
    python tools/xmodem_send.py --port COM5 --baud 115200 --file app.bin [--xmodem-1k] [--verbose]

  流输出 (host 端到端自测: 纯字节流经 tools/xmodem_host_recv.c 喂 et_xmodem):
    python tools/xmodem_send.py --emit --file app.bin [--xmodem-1k] > stream.bin

协议要点 (与 et_xmodem.c 对齐):
  - 数据块: SOH(0x01)+块号+~块号+128B, 或 STX(0x02)+...+1024B(--xmodem-1k);
  - CRC16-CCITT(poly 0x1021, init 0)覆盖整块载荷, 高字节先发;
  - 尾块填充 0x1A(SUB); EOT 两段确认: 第一段收 NAK, 第二段收 ACK;
  - 起步握手: 收到 NAK(0x15)或 'C'(0x43)即开始发块。
"""
import argparse
import sys
import time

SOH, STX, EOT = 0x01, 0x02, 0x04
ACK, NAK, CAN = 0x06, 0x15, 0x18
CRC_CH = 0x43                      # 'C'
SUB = 0x1A                         # 尾块填充


def crc16_ccitt(data):
    """XMODEM CRC: poly 0x1021, init 0x0000, 无反射 —— 与 et_crc16_ccitt 一致"""
    crc = 0x0000
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def build_blocks(data, use_1k):
    """按 128/1024B 分块, 返回 [ (hdr, blk_no, payload), ... ], 尾块 0x1A 填充"""
    size = 1024 if use_1k else 128
    hdr = STX if use_1k else SOH
    blocks = []
    if len(data) == 0:
        data = bytes([SUB])         # 空文件也发一块, 便于对端收尾
    for off in range(0, len(data), size):
        chunk = data[off:off + size]
        if len(chunk) < size:
            chunk = chunk + bytes([SUB]) * (size - len(chunk))
        blocks.append((hdr, (len(blocks) % 256) + 1, chunk))
    return blocks


def block_bytes(hdr, blk_no, payload):
    crc = crc16_ccitt(payload)
    return bytes([hdr, blk_no & 0xFF, (~blk_no) & 0xFF]) + payload + \
        bytes([(crc >> 8) & 0xFF, crc & 0xFF])


def emit_stream(args):
    """--emit: 静态字节流 (块 + EOT×2), 供 host 端到端自测"""
    with open(args.file, 'rb') as f:
        data = f.read()
    out = bytearray()
    for hdr, blk_no, payload in build_blocks(data, args.xmodem_1k):
        out += block_bytes(hdr, blk_no, payload)
    out += bytes([EOT, EOT])        # 两段确认: 第一段 NAK, 第二段 ACK+DONE
    sys.stdout.buffer.write(out)
    return 0


def send_serial(args):
    import serial                    # pyserial; 仅串口模式需要
    with open(args.file, 'rb') as f:
        data = f.read()
    blocks = build_blocks(data, args.xmodem_1k)
    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    ser.reset_input_buffer()
    log = (lambda *a: print(*a, file=sys.stderr)) if args.verbose else \
        (lambda *a: None)

    def wait_start():
        """等接收器催块: NAK 或 'C'"""
        deadline = time.time() + args.timeout
        while time.time() < deadline:
            b = ser.read(1)
            if b and b[0] in (NAK, CRC_CH):
                return True
        return False

    def send_block(hdr, blk_no, payload):
        """发一块并等应答; NAK 重发, 上限 10 次"""
        frame = block_bytes(hdr, blk_no, payload)
        for retry in range(10):
            ser.write(frame)
            log("  block %3d send (retry=%d)" % (blk_no, retry))
            deadline = time.time() + 1.0
            while time.time() < deadline:
                b = ser.read(1)
                if not b:
                    continue
                if b[0] == ACK:
                    return True
                if b[0] == NAK:
                    break                       # 重发
                if b[0] == CAN:
                    raise RuntimeError("receiver CAN at block %d" % blk_no)
        return False

    if not wait_start():
        print("ERROR: no NAK/'C' within %ds (板上先执行 AT+UPGRADE)" %
              args.timeout, file=sys.stderr)
        return 1

    for hdr, blk_no, payload in blocks:
        if not send_block(hdr, blk_no, payload):
            print("ERROR: block %d not ACKed" % blk_no, file=sys.stderr)
            return 1

    # EOT 两段确认: 第一段预期 NAK, 第二段预期 ACK
    ser.write(bytes([EOT]))
    log("EOT#1 sent")
    deadline = time.time() + 1.0
    while time.time() < deadline:
        b = ser.read(1)
        if b and b[0] == NAK:
            break
    ser.write(bytes([EOT]))
    log("EOT#2 sent")
    deadline = time.time() + 1.0
    while time.time() < deadline:
        b = ser.read(1)
        if b and b[0] == ACK:
            log("transfer done: %d bytes in %d blocks" %
                (len(data), len(blocks)))
            ser.close()
            return 0
    print("ERROR: no ACK for EOT#2", file=sys.stderr)
    ser.close()
    return 1


def main():
    ap = argparse.ArgumentParser(description="XMODEM-CRC sender for "
                                 "Embedded_Tools et_xmodem")
    ap.add_argument('--port', help="串口名 (如 COM5 / /dev/ttyUSB0)")
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--file', required=True, help="待发送文件")
    ap.add_argument('--xmodem-1k', action='store_true',
                    help="1024B 大块 (对端需 ET_XM_1K=1 编译)")
    ap.add_argument('--emit', action='store_true',
                    help="不发串口, 字节流写 stdout (host 自测)")
    ap.add_argument('--timeout', type=int, default=30,
                    help="等 NAK/'C' 的秒数")
    ap.add_argument('--verbose', action='store_true')
    args = ap.parse_args()

    if args.emit:
        return emit_stream(args)
    if not args.port:
        ap.error("--port required (or use --emit)")
    return send_serial(args)


if __name__ == '__main__':
    sys.exit(main())
