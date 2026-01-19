#!/usr/bin/env python3

from build_complete import beep
from pathlib import Path
import subprocess
import sys


def run(cmd, cwd):
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def main():
    try:
        run(["cmake", "-DCMAKE_BUILD_TYPE=Debug", ".."], Path("BUILD"))
        run(["cmake", "--build", ".", "--target", "Tests"], Path("BUILD"))
    except Exception:
        beep(success=False)
        raise

    beep(success=True)


if __name__ == "__main__":
    main()
