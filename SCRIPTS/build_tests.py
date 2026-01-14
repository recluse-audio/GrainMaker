#!/usr/bin/env python3
"""
Cross-platform CMake build script (macOS / Windows / Linux).

Equivalent to:
  pushd BUILD
  cmake -DCMAKE_BUILD_TYPE=Debug ..
  cmake --build . --target Tests
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

    # Configure (Debug)
    run(
        ["cmake", "-DCMAKE_BUILD_TYPE=Debug", ".."],
        cwd=build_dir,
    )

    # Build target
    build_cmd = ["cmake", "--build", ".", "--target", "Tests"]

    # Visual Studio / multi-config handling
    if sys.platform.startswith("win"):
        build_cmd += ["--config", "Debug"]

    run(build_cmd, cwd=build_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
