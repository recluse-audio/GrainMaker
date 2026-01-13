#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] if (Path(__file__).parent.name.lower() in {"scripts", "script"}) else Path.cwd()
BUILD_DIR = ROOT / "BUILD"
TARGET = "GrainMaker"
BUILD_TYPE = os.environ.get("CMAKE_BUILD_TYPE", "Debug")

def run(cmd, cwd=None):
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=cwd, check=True)

def is_multi_config_generator() -> bool:
    gen = (os.environ.get("CMAKE_GENERATOR") or "").lower()
    return ("visual studio" in gen) or ("xcode" in gen)

def main():
    # Mirrors: rm -rf BUILD && mkdir BUILD
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    # Configure (your script: cmake ..)
    cfg_cmd = ["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR)]
    if not is_multi_config_generator():
        cfg_cmd += [f"-DCMAKE_BUILD_TYPE={BUILD_TYPE}"]
    run(cfg_cmd)

    # Build GrainMaker (your Tests line was commented out; keeping that behavior)
    build_cmd = ["cmake", "--build", str(BUILD_DIR), "--target", TARGET]
    if is_multi_config_generator():
        build_cmd += ["--config", BUILD_TYPE]
    run(build_cmd)

if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
