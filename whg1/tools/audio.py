#!/usr/bin/env python3
"""Convert WHG1 exported SWF sounds to 16-bit PCM mono WAV for 3DS ndsp."""
import os
import shutil
import subprocess

SRC_DIR = "whg1/out/export/sounds"
DST_DIR = "whg1/romfs/sfx"

SOUND_MAP = [
    ("153.mp3", "die.wav",      "22050"),
    ("152.mp3", "coin.wav",     "22050"),
    ("132.mp3", "win.wav",      "22050"),
    ("129.mp3", "win_game.wav", "22050"),
    ("106.mp3", "music.wav",    "22050"),
]


def main():
    os.makedirs(DST_DIR, exist_ok=True)
    for src_name, dst_name, rate in SOUND_MAP:
        src = os.path.join(SRC_DIR, src_name)
        dst = os.path.join(DST_DIR, dst_name)
        if not os.path.exists(src):
            print(f"Warning: missing {src}")
            continue
        subprocess.run([
            "ffmpeg", "-y", "-v", "error",
            "-i", src,
            "-ac", "1", "-ar", rate, "-c:a", "pcm_s16le",
            dst
        ], check=True)
        print(f"  {dst_name:14s} ({os.path.getsize(dst) / 1024:.1f} KB)")

    # Reuse pause.wav and key.wav (silent/pause) from WHG2 if present so audio.c loads cleanly
    for extra in ("pause.wav", "key.wav"):
        src_extra = os.path.join("romfs/sfx", extra)
        dst_extra = os.path.join(DST_DIR, extra)
        if os.path.exists(src_extra) and not os.path.exists(dst_extra):
            shutil.copy2(src_extra, dst_extra)
            print(f"  {extra:14s} (copied from WHG2)")


if __name__ == "__main__":
    main()
