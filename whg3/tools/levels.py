#!/usr/bin/env python3
"""Extract all 48 WHG3 rooms from SWF into whg3/romfs/levels/lN_CR.lvl."""
import math
import os
import re
import struct
import zlib
from io import BytesIO
from PIL import Image
import resvg_py

SWF_PATH = "input/worldshardestgame3.swf"
EXPORT_DIR = "whg3/out/export"
OUT_DIR = "whg3/romfs/levels"
STAGE_W = 900
STAGE_H = 600


def read_tags(buf, start, end):
    tags = []
    p = start
    while p < end:
        hdr = struct.unpack_from("<H", buf, p)[0]
        p += 2
        code = hdr >> 6
        length = hdr & 0x3F
        if length == 0x3F:
            length = struct.unpack_from("<I", buf, p)[0]
            p += 4
        tags.append((code, buf[p : p + length]))
        p += length
        if code == 0:
            break
    return tags


def read_cstr(b, p):
    e = b.index(0, p)
    return b[p:e].decode("latin1"), e + 1


def read_rect(b, p):
    nb = b[p] >> 3
    bitpos = 5

    def gsb(n):
        nonlocal bitpos
        v = 0
        for _ in range(n):
            v = (v << 1) | ((b[p + (bitpos >> 3)] >> (7 - (bitpos & 7))) & 1)
            bitpos += 1
        if n > 0 and (v & (1 << (n - 1))):
            v -= 1 << n
        return v

    xmin, xmax, ymin, ymax = gsb(nb), gsb(nb), gsb(nb), gsb(nb)
    return xmin / 20.0, xmax / 20.0, ymin / 20.0, ymax / 20.0


def _parse_po_common(b, p, flags, flags2=0):
    if (flags2 & 8) or ((flags2 & 16) and (flags & 2)):
        _, p = read_cstr(b, p)
    chid = struct.unpack_from("<H", b, p)[0] if (flags & 2) else None
    if flags & 2:
        p += 2
    mat = None
    if flags & 4:
        bitpos = 0

        def gb(n):
            nonlocal bitpos
            v = 0
            for _ in range(n):
                v = (v << 1) | ((b[p + (bitpos >> 3)] >> (7 - (bitpos & 7))) & 1)
                bitpos += 1
            return v

        def gsb(n):
            v = gb(n)
            if n > 0 and (v & (1 << (n - 1))):
                v -= 1 << n
            return v

        sx, sy, r0, r1 = 1.0, 1.0, 0.0, 0.0
        if gb(1):
            nb = gb(5)
            sx = gsb(nb) / 65536.0
            sy = gsb(nb) / 65536.0
        if gb(1):
            nb = gb(5)
            r0 = gsb(nb) / 65536.0
            r1 = gsb(nb) / 65536.0
        nb = gb(5)
        tx = gsb(nb) / 20.0
        ty = gsb(nb) / 20.0
        p += (bitpos + 7) // 8
        mat = (sx, sy, r0, r1, tx, ty)
    if flags & 8:
        bitpos = 0

        def gb2(n):
            nonlocal bitpos
            v = 0
            for _ in range(n):
                v = (v << 1) | ((b[p + (bitpos >> 3)] >> (7 - (bitpos & 7))) & 1)
                bitpos += 1
            return v

        ha, hm = gb2(1), gb2(1)
        nb = gb2(4)
        if hm:
            gb2(nb * 4)
        if ha:
            gb2(nb * 4)
        p += (bitpos + 7) // 8
    if flags & 16:
        p += 2
    name = read_cstr(b, p)[0] if (flags & 32) else None
    return chid, mat, name


def parse_po2(b):
    flags = b[0]
    depth = struct.unpack_from("<H", b, 1)[0]
    ch, mat, nm = _parse_po_common(b, 3, flags, 0)
    return depth, ch, mat, nm


def parse_po3(b):
    flags = b[0]
    flags2 = b[1]
    depth = struct.unpack_from("<H", b, 2)[0]
    ch, mat, nm = _parse_po_common(b, 4, flags, flags2)
    return depth, ch, mat, nm


def mat_mul(a, b):
    asx, asy, ar0, ar1, atx, aty = a
    bsx, bsy, br0, br1, btx, bty = b
    return (
        asx * bsx + ar1 * br0,
        ar0 * br1 + asy * bsy,
        ar0 * bsx + asy * br0,
        asx * br1 + ar1 * bsy,
        asx * btx + ar1 * bty + atx,
        ar0 * btx + asy * bty + aty,
    )


def greedy_rects(mask, w, h):
    visited = [[False] * w for _ in range(h)]
    rects = []
    for y in range(h):
        x = 0
        while x < w:
            if mask[y][x] and not visited[y][x]:
                rw = 0
                while x + rw < w and mask[y][x + rw] and not visited[y][x + rw]:
                    rw += 1
                rh = 1
                while y + rh < h:
                    ok = True
                    for xx in range(x, x + rw):
                        if not mask[y + rh][xx] or visited[y + rh][xx]:
                            ok = False
                            break
                    if not ok:
                        break
                    rh += 1
                for yy in range(y, y + rh):
                    for xx in range(x, x + rw):
                        visited[yy][xx] = True
                rects.append((x, y, rw, rh))
                x += rw
            else:
                x += 1
    return rects


def rasterize_shape_to_stage(shape_id, mat, shapes_bounds, stage_mask, w=STAGE_W, h=STAGE_H):
    svg_path = os.path.join(EXPORT_DIR, f"{shape_id}.svg")
    if not os.path.exists(svg_path):
        svg_path = os.path.join(EXPORT_DIR, "shapes", f"{shape_id}.svg")
    if not os.path.exists(svg_path):
        return
    png_bytes = resvg_py.svg_to_bytes(svg_path=svg_path)
    im = Image.open(BytesIO(png_bytes)).convert("RGBA")
    xmin, xmax, ymin, ymax = shapes_bounds[shape_id]
    sx, sy, r0, r1, tx, ty = mat
    sw, sh = im.size
    if sw == 0 or sh == 0:
        return
    tw = max(1, int(round(sw * abs(sx))))
    th = max(1, int(round(sh * abs(sy))))
    if (tw, th) != (sw, sh):
        im = im.resize((tw, th), Image.NEAREST)
    ox = int(round(tx + xmin * sx))
    oy = int(round(ty + ymin * sy))
    alpha = im.getchannel("A")
    px = alpha.load()
    for iy in range(th):
        sy_pos = oy + iy
        if 0 <= sy_pos < h:
            row = stage_mask[sy_pos]
            for ix in range(tw):
                sx_pos = ox + ix
                if 0 <= sx_pos < w and px[ix, iy] > 128:
                    row[sx_pos] = True


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(SWF_PATH, "rb") as f:
        sig = f.read(3)
        f.read(1)
        struct.unpack("<I", f.read(4))
        raw = zlib.decompress(f.read()) if sig == b"CWS" else f.read()

    nbits = raw[0] >> 3
    pos = ((5 + nbits * 4) + 7) // 8 + 4
    tags = read_tags(raw, pos, len(raw))

    shapes_bounds = {}
    sprites = {}
    for code, data in tags:
        if code in (2, 22, 32, 83):
            chid = struct.unpack_from("<H", data, 0)[0]
            shapes_bounds[chid] = read_rect(data, 2)
        elif code == 39:
            chid = struct.unpack_from("<H", data, 0)[0]
            fcount = struct.unpack_from("<H", data, 2)[0]
            sprites[chid] = (fcount, read_tags(data, 4, len(data)))

    sprite_frames_cache = {}

    def get_sprite_timeline_frames(sid):
        if sid in sprite_frames_cache:
            return sprite_frames_cache[sid]
        fcount, stags = sprites[sid]
        frames = []
        sdl = {}
        for c, d in stags:
            if c in (26, 70):
                dp, ch, mat, nm = parse_po2(d) if c == 26 else parse_po3(d)
                prev = sdl.get(dp, (None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0), None))
                sdl[dp] = (
                    ch if ch is not None else prev[0],
                    mat if mat is not None else prev[1],
                    nm if nm is not None else prev[2],
                )
            elif c == 28:
                sdl.pop(struct.unpack_from("<H", d, 0)[0], None)
            elif c == 1:
                frames.append(dict(sdl))
        res = frames if frames else [dict(sdl)]
        sprite_frames_cache[sid] = res
        return res

    def get_sprite_world_bounds(sid, parent_mat):
        if sid in shapes_bounds:
            xmin, xmax, ymin, ymax = shapes_bounds[sid]
            sx, sy, _, _, tx, ty = parent_mat
            x0, x1 = xmin * sx + tx, xmax * sx + tx
            y0, y1 = ymin * sy + ty, ymax * sy + ty
            return (min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1))
        if sid not in sprites:
            return None
        sframes = get_sprite_timeline_frames(sid)
        boxes = []
        for _, (ch, mat, _) in sframes[0].items():
            if ch is None:
                continue
            b = get_sprite_world_bounds(ch, mat_mul(parent_mat, mat))
            if b is not None:
                boxes.append(b)
        if not boxes:
            return None
        return (
            min(b[0] for b in boxes),
            min(b[1] for b in boxes),
            max(b[2] for b in boxes),
            max(b[3] for b in boxes),
        )

    def rasterize_recursive(sid, parent_mat, mask):
        if sid in shapes_bounds:
            rasterize_shape_to_stage(sid, parent_mat, shapes_bounds, mask)
            return
        if sid not in sprites:
            return
        sframes = get_sprite_timeline_frames(sid)
        for _, (ch, mat, _) in sorted(sframes[0].items()):
            if ch is not None:
                rasterize_recursive(ch, mat_mul(parent_mat, mat), mask)

    def collect_enemy_positions_at_frame(sid, parent_mat, t_frame, out_list):
        if sid not in sprites:
            return
        sframes = get_sprite_timeline_frames(sid)
        cur = sframes[t_frame % len(sframes)]
        for dp, (ch, mat, nm) in sorted(cur.items()):
            if ch is None:
                continue
            wmat = mat_mul(parent_mat, mat)
            if ch in sprites:
                collect_enemy_positions_at_frame(ch, wmat, t_frame, out_list)
            elif ch in shapes_bounds:
                xmin, xmax, ymin, ymax = shapes_bounds[ch]
                cx = (xmin + xmax) * 0.5
                cy = (ymin + ymax) * 0.5
                wx = wmat[0] * cx + wmat[3] * cy + wmat[4]
                wy = wmat[2] * cx + wmat[1] * cy + wmat[5]
                out_list.append((wx, wy))

    def estimate_enemy_period(sid):
        if sid not in sprites:
            return 1
        sframes = get_sprite_timeline_frames(sid)
        p = len(sframes)
        for dp, (ch, mat, nm) in sframes[0].items():
            if ch in sprites:
                cp = estimate_enemy_period(ch)
                p = math.lcm(p, cp)
                if p > 600:
                    return 600
        return max(1, min(p, 600))

    # Walk Sprite 1068 (game rooms)
    _, gtags = sprites[1068]
    fr = 1
    labels = {}
    dl = {}
    rooms = []
    for c, d in gtags:
        if c == 43:
            labels[fr] = read_cstr(d, 0)[0]
        elif c in (26, 70):
            dp, ch, mat, nm = parse_po2(d) if c == 26 else parse_po3(d)
            prev = dl.get(dp, (None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0), None))
            dl[dp] = (
                ch if ch is not None else prev[0],
                mat if mat is not None else prev[1],
                nm if nm is not None else prev[2],
            )
        elif c == 28:
            dl.pop(struct.unpack_from("<H", d, 0)[0], None)
        elif c == 1:
            lbl = labels.get(fr, "")
            if re.match(r"^l[1-5]_[A-E][1-5]$", lbl):
                rooms.append((lbl, dict(dl)))
            fr += 1

    print(f"Found {len(rooms)} rooms in Sprite 1068.")
    for ridx, (lbl, dlist) in enumerate(rooms, 1):
        walls_id, walls_mat = None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        enemies_id, enemies_mat = None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        coins_id, coins_mat = None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        keys_id, keys_mat = None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        checks = []

        for dp, (ch, mat, nm) in sorted(dlist.items()):
            if nm == "walls":
                walls_id, walls_mat = ch, mat
            elif nm == "enemies":
                enemies_id, enemies_mat = ch, mat
            elif nm == "coins":
                coins_id, coins_mat = ch, mat
            elif nm == "keys":
                keys_id, keys_mat = ch, mat
            elif nm and re.match(r"^c\d+$", nm):
                cidx = int(nm[1:])
                b = get_sprite_world_bounds(ch, mat)
                if b is not None:
                    x = int(round(b[0]))
                    y = int(round(b[1]))
                    w = int(round(b[2] - b[0]))
                    h = int(round(b[3] - b[1]))
                    checks.append((cidx, x, y, w, h))

        # Walls & Doors
        stage_mask = [[False] * STAGE_W for _ in range(STAGE_H)]
        doors = []
        if walls_id and walls_id in sprites:
            wframes = get_sprite_timeline_frames(walls_id)
            for dp, (ch, mat, nm) in sorted(wframes[0].items()):
                if ch is None:
                    continue
                wmat = mat_mul(walls_mat, mat)
                if nm and (nm == "gd" or (nm.startswith("d") and nm[1:].isdigit())):
                    did = 255 if nm == "gd" else int(nm[1:])
                    dmask = [[False] * STAGE_W for _ in range(STAGE_H)]
                    rasterize_recursive(ch, wmat, dmask)
                    for rx, ry, rw, rh in greedy_rects(dmask, STAGE_W, STAGE_H):
                        doors.append((did, rx, ry, rw, rh))
                else:
                    rasterize_recursive(ch, wmat, stage_mask)

        wall_rects = greedy_rects(stage_mask, STAGE_W, STAGE_H)

        # Coins (sorted by c0, c1, ...)
        coins = []
        if coins_id and coins_id in sprites:
            cframes = get_sprite_timeline_frames(coins_id)
            clist = []
            for dp, (ch, mat, nm) in sorted(cframes[0].items()):
                if nm and re.match(r"^c\d+$", nm):
                    wmat = mat_mul(coins_mat, mat)
                    clist.append((int(nm[1:]), int(round(wmat[4])), int(round(wmat[5]))))
            clist.sort(key=lambda x: x[0])
            coins = [(cx, cy) for _, cx, cy in clist]

        # Keys (k0, k1, ...)
        keys = []
        if keys_id and keys_id in sprites:
            kframes = get_sprite_timeline_frames(keys_id)
            for dp, (ch, mat, nm) in sorted(kframes[0].items()):
                if nm and re.match(r"^k\d+$", nm):
                    wmat = mat_mul(keys_mat, mat)
                    keys.append((int(nm[1:]), int(round(wmat[4])), int(round(wmat[5]))))

        # Enemies
        enemy_tracks = []
        if enemies_id and enemies_id in sprites:
            period = estimate_enemy_period(enemies_id)
            first_pos = []
            collect_enemy_positions_at_frame(enemies_id, enemies_mat, 0, first_pos)
            n_en = len(first_pos)
            if n_en > 0:
                tracks = [[] for _ in range(n_en)]
                for t in range(period):
                    pos_t = []
                    collect_enemy_positions_at_frame(enemies_id, enemies_mat, t, pos_t)
                    for ei in range(min(n_en, len(pos_t))):
                        tracks[ei].append(pos_t[ei])
                for tr in tracks:
                    if len(set((round(x, 1), round(y, 1)) for x, y in tr)) <= 1:
                        enemy_tracks.append([tr[0]])
                    else:
                        enemy_tracks.append(tr)

        out_path = os.path.join(OUT_DIR, f"{lbl}.lvl")
        with open(out_path, "wb") as out:
            out.write(struct.pack("<4sHHHHHH", b"WHG2", 1, ridx, STAGE_W, STAGE_H, 0, 0))
            out.write(struct.pack("<H", len(wall_rects)))
            for rx, ry, rw, rh in wall_rects:
                out.write(struct.pack("<hhhh", rx, ry, rw, rh))
            out.write(struct.pack("<H", len(doors)))
            for did, rx, ry, rw, rh in doors:
                out.write(struct.pack("<Hhhhh", did, rx, ry, rw, rh))
            out.write(struct.pack("<H", len(checks)))
            for kind, x, y, w, h in checks:
                out.write(struct.pack("<Hhhhh", kind, x, y, w, h))
            out.write(struct.pack("<H", len(coins)))
            for cx, cy in coins:
                out.write(struct.pack("<hh", cx, cy))
            out.write(struct.pack("<H", len(keys)))
            for did, kx, ky in keys:
                out.write(struct.pack("<Hhh", did, kx, ky))
            out.write(struct.pack("<H", len(enemy_tracks)))
            for tr in enemy_tracks:
                out.write(struct.pack("<HH", len(tr), 65))
                for ex, ey in tr:
                    out.write(struct.pack("<hh", int(round(ex * 2)), int(round(ey * 2))))

        print(
            f"[{ridx:02d}] {lbl:6s}: walls={len(wall_rects):3d}, doors={len(doors)}, "
            f"checks={len(checks)}, coins={len(coins):3d}, keys={len(keys)}, "
            f"enemies={len(enemy_tracks):2d} (max_f={max((len(t) for t in enemy_tracks), default=0)})"
        )


if __name__ == "__main__":
    main()
