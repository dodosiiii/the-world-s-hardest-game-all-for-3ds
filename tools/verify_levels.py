#!/usr/bin/env python3
"""Phase 3 validation: verify all 54 binary .lvl files."""
import os
import struct

OUT_DIR = "romfs/levels"
total_bytes = 0
for idx in range(1, 55):
    path = os.path.join(OUT_DIR, f"{idx:02d}.lvl")
    data = open(path, "rb").read()
    total_bytes += len(data)
    magic, ver, lid, w, h, flags, res = struct.unpack_from("<4sHHHHHH", data, 0)
    assert magic == b"WHG2" and ver == 1 and lid == idx, f"Bad header in {path}"
    p = 16
    nw = struct.unpack_from("<H", data, p)[0]; p += 2 + nw * 8
    nd = struct.unpack_from("<H", data, p)[0]; p += 2 + nd * 10
    nc = struct.unpack_from("<H", data, p)[0]; p += 2 + nc * 10
    ncoins = struct.unpack_from("<H", data, p)[0]; p += 2 + ncoins * 4
    nk = struct.unpack_from("<H", data, p)[0]; p += 2 + nk * 6
    ne = struct.unpack_from("<H", data, p)[0]; p += 2
    for _ in range(ne):
        nf, rad = struct.unpack_from("<HH", data, p); p += 4 + nf * 4
    assert p == len(data), f"Size mismatch in {path}: parsed {p} != {len(data)}"
print(f"Verified 54/54 .lvl files OK! Total size: {total_bytes / 1024:.1f} KB")
