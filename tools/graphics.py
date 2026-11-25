#!/usr/bin/env python3
"""Phase 2: Build 3DS spritesheet (.t3s -> .t3x) and bitmap font (.bcfnt) from FFDec export."""
import os
import shutil
import subprocess
from PIL import Image

EXPORT_DIR = "out/export"
GFX_DIR = "gfx"
SPRITES_OUT = os.path.join(GFX_DIR, "sprites")
ROMFS_DIR = "romfs"

SPRITE_MAP = [
    ("player_1.png", "sprites/DefineSprite_147/1.png"),
    ("player_2.png", "sprites/DefineSprite_147/2.png"),
    ("player_3.png", "sprites/DefineSprite_147/3.png"),
    ("player_4.png", "sprites/DefineSprite_147/4.png"),
    ("player_5.png", "sprites/DefineSprite_147/5.png"),
    ("player_6.png", "sprites/DefineSprite_147/6.png"),
    ("enemy_blue.png", "sprites/DefineSprite_140/1.png"),
    ("coin.png", "sprites/DefineSprite_154/1.png"),
    ("key.png", "sprites/DefineSprite_348/1.png"),
]

def main():
    os.makedirs(SPRITES_OUT, exist_ok=True)
    os.makedirs(os.path.join(ROMFS_DIR, "gfx"), exist_ok=True)

    t3s_lines = ["--atlas -f rgba8888 -z auto"]
    print("=== Preparing sprites ===")
    for dst_name, src_rel in SPRITE_MAP:
        src_path = os.path.join(EXPORT_DIR, src_rel)
        dst_path = os.path.join(SPRITES_OUT, dst_name)
        if not os.path.exists(src_path):
            raise FileNotFoundError(f"Missing exported sprite: {src_path}")
        im = Image.open(src_path).convert("RGBA")
        im.save(dst_path)
        t3s_lines.append(f"sprites/{dst_name}")
        print(f"  {dst_name:16s} {im.size[0]}x{im.size[1]}")

    t3s_path = os.path.join(GFX_DIR, "sprites.t3s")
    with open(t3s_path, "w", newline="\n") as f:
        f.write("\n".join(t3s_lines) + "\n")
    print(f"Wrote {t3s_path} ({len(SPRITE_MAP)} sprites)")

    # Generate bitmap font (.bcfnt) - prefer full system Arial/SegoeUI so all glyphs exist
    ttf_candidates = [
        r"C:\Windows\Fonts\arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        os.path.join(EXPORT_DIR, "fonts", "21_Arial.ttf"),
    ]
    ttf_path = next((p for p in ttf_candidates if os.path.exists(p)), None)
    bcfnt_path = os.path.join(ROMFS_DIR, "font.bcfnt")
    mkbcfnt = r"C:\devkitPro\tools\bin\mkbcfnt.exe"
    if not os.path.exists(mkbcfnt):
        mkbcfnt = "mkbcfnt"
    if ttf_path:
        subprocess.run([mkbcfnt, "-s", "14", "-o", bcfnt_path, ttf_path], check=True)
        print(f"Generated {bcfnt_path} from {ttf_path} ({os.path.getsize(bcfnt_path)} bytes)")
    else:
        print("Warning: no TTF font found, skipping .bcfnt generation")

if __name__ == "__main__":
    main()
