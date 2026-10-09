# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations


#: Board Manager-backed LCD touch controller.
class Touch:
    #: Initialize the board's LCD touch controller; raises OSError on unsupported boards.
    def __init__(self) -> None: ...
    def __del__(self) -> None: ...
    #: Return up to five (x, y) points in physical display coordinates; () means released.
    def read(self) -> tuple[tuple[int, int], ...]: ...
    #: Release touch resources. Repeated calls are safe.
    def deinit(self) -> None: ...
