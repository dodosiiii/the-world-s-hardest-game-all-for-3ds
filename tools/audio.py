#!/usr/bin/env python3
"""Phase 7: Convert exported SWF sounds to 16-bit PCM mono WAV for 3DS ndsp."""
import os
import subprocess

SRC_DIR = "out/export/sounds"
DST_DIR = "romfs/sfx"

SOUND_MAP = [
    ("7_die.mp3",      "die.wav",      "22050"),
    ("8_coin.mp3",     "coin.wav",     "22050"),
    ("6_key.mp3",      "key.wav",      "22050"),
    ("2_win.mp3",      "win.wav",      "22050"),
    ("1_win game.mp3", "win_game.wav", "22050"),
    ("4_pause.mp3",    "pause.wav",    "22050"),
    ("5_music.mp3",    "music.wav",    "22050"),
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
