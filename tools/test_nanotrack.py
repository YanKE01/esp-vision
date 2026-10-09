#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Host checks for INT8 tracking math and LCD/image coordinate mapping."""

import ast
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="esp-vision-nanotrack-") as tmp:
        binary = Path(tmp) / "nanotrack_test"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-I" + str(ROOT / "platform"),
            str(ROOT / "tools/host_tests/nanotrack_math_test.cpp"), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    path = ROOT / "example/03-Machine-Learning/00-ESP-DL/nanotrack_touch.py"
    tree = ast.parse(path.read_text())
    tree.body = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
    namespace = {}
    exec(compile(tree, str(path), "exec"), namespace)
    map_touch = namespace["touch_to_image"]
    # QVGA centered on the Function EV Board's 1024x600 panel.
    assert map_touch((112, 0), 112, 0, 2.5, 320, 240) == (0, 0)
    assert map_touch((512, 300), 112, 0, 2.5, 320, 240) == (160, 120)
    assert map_touch((111, 100), 112, 0, 2.5, 320, 240) is None
    assert map_touch((912, 100), 112, 0, 2.5, 320, 240) is None
    assert map_touch((1023, 599), 112, 0, 2.5, 320, 240, clamp=True) == (319, 239)
    # Reverse-direction drags still produce an in-bounds inclusive rectangle.
    assert namespace["selection"]((319, 239), (0, 0)) == (0, 0, 320, 240)
    print("NanoTrack math and touch-coordinate checks passed.")


if __name__ == "__main__":
    main()
