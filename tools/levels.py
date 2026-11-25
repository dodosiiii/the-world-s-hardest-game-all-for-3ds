#!/usr/bin/env python3
"""Phase 3: Extract all 54 WHG2 levels from SWF into romfs/levels/NN.lvl."""
import math
import os
import re
import struct
import zlib
from io import BytesIO
from PIL import Image
import resvg_py

SWF_PATH = "input/theworldshardestgame2.swf"
EXPORT_DIR = "out/export"
OUT_DIR = "romfs/levels"


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


def parse_po2(b):
    flags = b[0]
    depth = struct.unpack_from("<H", b, 1)[0]
    p = 3
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
    return depth, chid, mat, name


def mat_mul(a, b):
    # (sx, sy, r0, r1, tx, ty) where x' = sx*x + r1*y + tx, y' = r0*x + sy*y + ty
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
    """Decompose a boolean 2D grid (h x w) into disjoint axis-aligned rectangles."""
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


def rasterize_shape_to_stage(shape_id, mat, shapes_bounds, stage_mask, w=700, h=500):
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
    # Scale image by sx, sy if needed
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

    # Parse key->door mapping from exported scripts
    key_door_map = {}  # (frame_num, place_name_or_chid) -> door_id
    for root_dir, _, files in os.walk(os.path.join(EXPORT_DIR, "scripts")):
        for fn in files:
            if "enterFrame" in fn and "PlaceObject2_348_" in root_dir:
                m_f = re.search(r"frame_(\d+)", root_dir)
                if not m_f:
                    continue
                fnum = int(m_f.group(1))
                with open(os.path.join(root_dir, fn), encoding="utf-8", errors="ignore") as sf:
                    txt = sf.read()
                m_d = re.search(r"door(\d+)\.play\(\)", txt)
                if m_d:
                    key_door_map.setdefault(fnum, []).append(int(m_d.group(1)))

    # Walk main timeline
    frame = 1
    labels = {}
    dl = {}
    level_entries = []
    for code, data in tags:
        if code == 43:
            labels[frame] = read_cstr(data, 0)[0]
        elif code == 26:
            d, ch, mat, nm = parse_po2(data)
            prev = dl.get(d, (None, (1.0, 1.0, 0.0, 0.0, 0.0, 0.0), None))
            dl[d] = (
                ch if ch is not None else prev[0],
                mat if mat is not None else prev[1],
                nm if nm is not None else prev[2],
            )
        elif code == 28:
            dl.pop(struct.unpack_from("<H", data, 0)[0], None)
        elif code == 1:
            lbl = labels.get(frame, "")
            if lbl.startswith("level-") or lbl.startswith("tutorial-"):
                level_entries.append((lbl, frame, dict(dl)))
            frame += 1

    # Order: level-1..50 -> id 1..50, tutorial-1..4 -> id 51..54
    def sort_key(entry):
        lbl = entry[0]
        if lbl.startswith("level-"):
            return (0, int(lbl.split("-")[1]))
        return (1, int(lbl.split("-")[1]))

    level_entries.sort(key=sort_key)

    sprite_frames_cache = {}

    def get_sprite_timeline_frames(sid):
        """Return list of dicts {depth: (chid, mat, name)} per frame of sprite."""
        if sid in sprite_frames_cache:
            return sprite_frames_cache[sid]
        fcount, stags = sprites[sid]
        frames = []
        sdl = {}
        for c, d in stags:
            if c == 26:
                dp, ch, mat, nm = parse_po2(d)
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

    def collect_enemy_positions_at_frame(sid, parent_mat, t_frame, out_list):
        """Recursively collect leaf enemy shapes' world centers at frame t_frame."""
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
        """Estimate LCM period of a sprite and its child tween sprites (capped at 600)."""
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

    for idx, (lbl, fnum, dlist) in enumerate(level_entries, 1):
        walls_id = None
        walls_mat = (1.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        enemies_id = None
        enemies_mat = (1.0, 1.0, 0.0, 0.0, 0.0, 0.0)
        checks = []
        coins = []
        keys = []

        for dp, (ch, mat, nm) in sorted(dlist.items()):
            if nm in ("noCheat_walls", "walls"):
                walls_id, walls_mat = ch, mat
            elif nm in ("noCheat_enemies", "enemies"):
                enemies_id, enemies_mat = ch, mat
            elif nm and nm.startswith("check"):
                kind = 1 if nm == "check1" else (2 if nm == "check2" else 3)
                sx, sy, _, _, tx, ty = mat
                w = int(round(50 * abs(sx)))
                h = int(round(50 * abs(sy)))
                x = int(round(tx - w / 2.0))
                y = int(round(ty - h / 2.0))
                checks.append((kind, x, y, w, h))
            elif ch == 154:
                coins.append((int(round(mat[4])), int(round(mat[5]))))
            elif ch == 348:
                door_id = 1
                if nm and nm.startswith("key") and nm[3:].isdigit():
                    door_id = int(nm[3:])
                keys.append((door_id, int(round(mat[4])), int(round(mat[5]))))

        # Walls & doors
        stage_mask = [[False] * 700 for _ in range(500)]
        doors = []
        if walls_id and walls_id in sprites:
            wframes = get_sprite_timeline_frames(walls_id)
            for dp, (ch, mat, nm) in sorted(wframes[0].items()):
                if ch is None:
                    continue
                wmat = mat_mul(walls_mat, mat)
                if nm and nm.startswith("door") and ch in sprites:
                    did = int(nm[4:]) if nm[4:].isdigit() else 1
                    # Find shape bounds of door frame 1
                    dframes = get_sprite_timeline_frames(ch)
                    dmask = [[False] * 700 for _ in range(500)]
                    for _, (dch, dmat, _) in dframes[0].items():
                        if dch in shapes_bounds:
                            rasterize_shape_to_stage(dch, mat_mul(wmat, dmat), shapes_bounds, dmask)
                    drects = greedy_rects(dmask, 700, 500)
                    for rx, ry, rw, rh in drects:
                        doors.append((did, rx, ry, rw, rh))
                elif ch in shapes_bounds:
                    rasterize_shape_to_stage(ch, wmat, shapes_bounds, stage_mask)
                elif ch in sprites:
                    # Nested static wall sprite
                    subframes = get_sprite_timeline_frames(ch)
                    for _, (sch, smat, _) in subframes[0].items():
                        if sch in shapes_bounds:
                            rasterize_shape_to_stage(sch, mat_mul(wmat, smat), shapes_bounds, stage_mask)

        wall_rects = greedy_rects(stage_mask, 700, 500)

        # Enemies
        enemy_tracks = []  # list of list of (x, y)
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
                # Compress static enemies to 1 frame
                for tr in tracks:
                    if len(set((round(x, 1), round(y, 1)) for x, y in tr)) <= 1:
                        enemy_tracks.append([tr[0]])
                    else:
                        enemy_tracks.append(tr)

        # Write binary .lvl
        out_path = os.path.join(OUT_DIR, f"{idx:02d}.lvl")
        with open(out_path, "wb") as out:
            out.write(struct.pack("<4sHHHHHH", b"WHG2", 1, idx, 700, 500, 0, 0))
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
                out.write(struct.pack("<HH", len(tr), 65))  # radius 6.5 * 10 = 65
                for ex, ey in tr:
                    out.write(struct.pack("<hh", int(round(ex * 2)), int(round(ey * 2))))

        print(
            f"[{idx:02d}] {lbl:11s}: walls={len(wall_rects):3d}, doors={len(doors)}, "
            f"checks={len(checks)}, coins={len(coins):2d}, keys={len(keys)}, "
            f"enemies={len(enemy_tracks):2d} (max_f={max((len(t) for t in enemy_tracks), default=0)})"
        )


if __name__ == "__main__":
    main()
