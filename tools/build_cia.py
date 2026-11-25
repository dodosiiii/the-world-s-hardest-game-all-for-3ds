#!/usr/bin/env python3
"""Phase 9: End-to-end user script to build whg2-3ds.3dsx and whg2-3ds.cia from a user-supplied SWF."""
import argparse
import os
import platform
import shutil
import subprocess
import sys


def find_ffdec():
    for candidate in ("tools/ffdec/ffdec-cli.exe", "tools/ffdec/ffdec-cli", "ffdec-cli"):
        if shutil.which(candidate) or os.path.exists(candidate):
            return candidate
    raise FileNotFoundError("JPEXS ffdec-cli not found in tools/ffdec/ or PATH.")


def main():
    parser = argparse.ArgumentParser(description="Build WHG2 3DS (.3dsx and .cia) from SWF")
    parser.add_argument("swf", nargs="?", default="input/theworldshardestgame2.swf",
                        help="Path to theworldshardestgame2.swf")
    args = parser.parse_args()

    if not os.path.exists(args.swf):
        print(f"Error: SWF not found at {args.swf}", file=sys.stderr)
        sys.exit(1)

    os.makedirs("input", exist_ok=True)
    target_swf = "input/theworldshardestgame2.swf"
    if os.path.abspath(args.swf) != os.path.abspath(target_swf):
        shutil.copy2(args.swf, target_swf)

    ffdec = find_ffdec()
    print("[1/5] Exporting SWF with FFDec...")
    subprocess.run([ffdec, "-export", "all", "out/export", target_swf], check=True)

    print("[2/5] Converting graphics...")
    subprocess.run([sys.executable, "tools/graphics.py"], check=True)

    print("[3/5] Converting 54 levels...")
    subprocess.run([sys.executable, "tools/levels.py"], check=True)

    print("[4/5] Converting audio...")
    subprocess.run([sys.executable, "tools/audio.py"], check=True)

    print("[5/5] Compiling .3dsx and .cia...")
    if platform.system() == "Windows":
        bash = r"C:\devkitPro\msys2\usr\bin\bash.exe"
        cwd_posix = os.getcwd().replace("\\", "/")
        if len(cwd_posix) >= 2 and cwd_posix[1] == ":":
            cwd_posix = "/" + cwd_posix[0].lower() + cwd_posix[2:]
        subprocess.run([bash, "-l", "-c", f"cd '{cwd_posix}' && make cia"], check=True)
    else:
        subprocess.run(["make", "cia"], check=True)

    print("Done! Generated whg2-3ds.3dsx and whg2-3ds.cia")


if __name__ == "__main__":
    main()
