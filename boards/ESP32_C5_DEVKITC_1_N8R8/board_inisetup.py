# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

MAIN_PY = """\
import time

print("ESP-VISION ESP32_C5_DEVKITC_1_N8R8 ready")

while True:
    time.sleep_ms(1000)
"""

README_TXT = """\
ESP-VISION ESP32_C5_DEVKITC_1_N8R8

This bring-up firmware uses USB Serial/JTAG for IDE access and a generated
fake camera frame for preview and sensor API testing.
Edit main.py to run your Python vision script.
"""
