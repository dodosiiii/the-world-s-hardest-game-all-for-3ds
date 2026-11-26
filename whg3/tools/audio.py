#!/usr/bin/env python3
"""Convert WHG3 exported SWF sounds to 16-bit PCM mono WAV for 3DS ndsp."""
import os
import subprocess

SRC_DIR = "whg3/out/export/sounds"
DST_DIR = "whg3/romfs/sfx"

SOUND_MAP = [
    ("35_dieSound.mp3",   "die.wav",      "22050"),
    ("36_coinSound.mp3",  "coin.wav",     "22050"),
    ("33_keySound.mp3",   "key.wav",      "22050"),
    ("28_winSound.mp3",   "win.wav",      "22050"),
    ("34_doorSound.mp3",  "win_game.wav", "22050"),
    ("25_selectSound.mp3","pause.wav",    "22050"),
    ("32_musicSound.mp3", "music.wav",    "22050"),
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


if __name__ == "__main__":
    main()
