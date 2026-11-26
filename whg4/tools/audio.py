#!/usr/bin/env python3
"""Convert WHG4 exported SWF sounds to 16-bit PCM mono WAV for 3DS ndsp."""
import os
import subprocess

SRC_DIR = "whg4/out/export/sounds"
DST_DIR = "whg4/romfs/sfx"

SOUND_MAP = [
    ("71_dieEnemySound.mp3", "die.wav",      "22050"),
    ("73_coinSound.mp3",     "coin.wav",     "22050"),
    ("66_keySound.mp3",      "key.wav",      "22050"),
    ("61_winSound.mp3",      "win.wav",      "22050"),
    ("68_doorSound.mp3",     "win_game.wav", "22050"),
    ("57_selectSound.mp3",   "pause.wav",    "22050"),
    ("64_musicSound.mp3",    "music.wav",    "22050"),
    ("72_diamondSound.mp3",  "diamond.wav",  "22050"),
    ("74_checkSound.mp3",    "check.wav",    "22050"),
    ("69_dieWaterSound.mp3", "die_water.wav","22050"),
    ("70_diePitSound.mp3",   "die_pit.wav",  "22050"),
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
