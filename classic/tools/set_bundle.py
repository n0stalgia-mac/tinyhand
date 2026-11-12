#!/usr/bin/env python3
"""Set the Finder bundle bit on Retro68 MacBinary (.bin) output.

Retro68 writes finder flags as 0, so the Finder never reads BNDL/ICN#.
Byte 73 is the high 8 bits of the 16-bit Finder flags; 0x20 = kHasBundle.
"""
import os
import struct
import subprocess
import sys


def crc16(data):
    crc = 0
    for x in data:
        crc ^= x << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def patch_macbinary(path):
    with open(path, "rb") as f:
        buf = bytearray(f.read())
    if len(buf) < 128 or buf[0] != 0:
        raise SystemExit("not MacBinary: %s" % path)
    buf[73] = 0x20  # kHasBundle, inited clear
    crc = crc16(buf[:124])
    struct.pack_into(">H", buf, 124, crc)
    with open(path, "wb") as f:
        f.write(buf)


def patch_dsk(dsk, name):
    hattrib = None
    hmount = None
    humount = None
    for d in os.environ.get("PATH", "").split(os.pathsep):
        for tool, slot in (("hattrib", "hattrib"), ("hmount", "hmount"), ("humount", "humount")):
            p = os.path.join(d, tool)
            if os.path.isfile(p) and os.access(p, os.X_OK):
                if slot == "hattrib":
                    hattrib = p
                elif slot == "hmount":
                    hmount = p
                else:
                    humount = p
    if not (hattrib and hmount and humount):
        return
    subprocess.call([hmount, dsk], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.call([hattrib, "+b", name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.call([hattrib, "+b", ":" + name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.call([humount], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: set_bundle.py FILE.bin [FILE.dsk NAME]")
    patch_macbinary(sys.argv[1])
    if len(sys.argv) >= 4:
        patch_dsk(sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    main()
