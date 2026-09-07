#!/usr/bin/env python3
"""ETBI 镜像打包工具 (v1.6 走单③配套): 把任意 bin 封成 et_bootctl 可验证的升级镜像。

头布局 (32B, 小端, 与 storage/et_bootctl.h 对齐):
  0x00 magic      'ETBI'
  0x04 hdr_ver    1
  0x06 hdr_size   32 (u16)
  0x08 img_size   u32
  0x0C img_crc32  u32, zlib 族 (et_crc32 输出)
  0x10 ver        u32, 镜像版本 (demo 自检: 奇数=通过)
  0x18 reserved   u32 = 0
  0x1C hdr_crc32  u32, 覆盖头前 28 字节

用法:
  python tools/pack_image.py --in firmware.bin --ver 7 --out image.bin
随后: python tools/xmodem_send.py --port COM12 --file image.bin --verbose
"""
import argparse
import struct
import sys
import zlib

MAGIC = b'ETBI'
HDR_VER = 1
HDR_SIZE = 32


def pack(img: bytes, ver: int) -> bytes:
    hdr = bytearray(HDR_SIZE)
    hdr[0:4] = MAGIC
    hdr[4] = HDR_VER
    hdr[6:8] = struct.pack('<H', HDR_SIZE)
    hdr[8:12] = struct.pack('<I', len(img))
    hdr[12:16] = struct.pack('<I', zlib.crc32(img) & 0xFFFFFFFF)
    hdr[16:20] = struct.pack('<I', ver)
    hdr[28:32] = struct.pack('<I', zlib.crc32(bytes(hdr[:28])) & 0xFFFFFFFF)
    return bytes(hdr) + img


def main() -> int:
    ap = argparse.ArgumentParser(description='ETBI image packer')
    ap.add_argument('--in', dest='infile', required=True)
    ap.add_argument('--out', dest='outfile', required=True)
    ap.add_argument('--ver', type=int, default=1)
    a = ap.parse_args()
    img = open(a.infile, 'rb').read()
    if len(img) == 0:
        print('empty input', file=sys.stderr)
        return 1
    open(a.outfile, 'wb').write(pack(img, a.ver))
    print(f'packed {len(img)} B img (ver={a.ver}) -> {a.outfile} '
          f'({len(img) + HDR_SIZE} B)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
