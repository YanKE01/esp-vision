# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# Draw white touch trails on a black screen. Press Ctrl-C to stop.
import display
import image
import time
import lcd_touch


lcd = display.Display(backlight=100)
tp = None
try:
    tp = lcd_touch.Touch()
    frame = image.Image(lcd.width(), lcd.height(), image.RGB565)
    frame.clear()
    lcd.write(frame, fit=False)

    while True:
        points = tp.read()
        for x, y in points:
            frame.draw_circle(x, y, 3, color=(255, 255, 255), fill=True)
        if points:
            lcd.write(frame, fit=False)
        time.sleep_ms(20)
finally:
    try:
        if tp is not None:
            tp.deinit()
    finally:
        lcd.deinit()
