#!/usr/bin/env python3
"""
Cross-platform CMake build script (macOS / Windows / Linux).

Equivalent to:
  pushd BUILD
  cmake ..
  cmake --build . --target GrainMaker_VST3
  popd
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def run(cmd: list[str], cwd: Path) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def main() -> int:
    build_dir = Path("BUILD").resolve()
    if not build_dir.exists():
        raise FileNotFoundError("BUILD directory does not exist")

    # Configure
    run(["cmake", ".."], cwd=build_dir)

    # Build target
    build_cmd = ["cmake", "--build", ".", "--target", "GrainMaker_VST3"]

    # Visual Studio / multi-config handling
    if sys.platform.startswith("win"):
        build_cmd += ["--config", "Debug"]  # change to Release if needed

    run(build_cmd, cwd=build_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
