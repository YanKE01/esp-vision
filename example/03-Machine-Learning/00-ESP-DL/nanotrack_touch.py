# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Drag on the LCD to select one object; touch again to select a new target.

Copy the three full-INT8 exports to /sdcard/nanotrack_v2/ first.
Requires a board with configured LCD touch (P4 Function EV Board or S31 Mosaico).
"""

import display
import espdl
import lcd_touch
import sensor
import time


MODEL_DIR = "/sdcard/nanotrack_v2"
MIN_ROI = 10
LOST_FRAMES = 10


def touch_to_image(point, offset_x, offset_y, scale, width, height, clamp=False):
    x = (point[0] - offset_x) / scale
    y = (point[1] - offset_y) / scale
    if not clamp and (x < 0 or y < 0 or x >= width or y >= height):
        return None
    return (max(0, min(width - 1, int(x))), max(0, min(height - 1, int(y))))


def selection(start, end):
    return (min(start[0], end[0]), min(start[1], end[1]),
            abs(end[0] - start[0]) + 1, abs(end[1] - start[1]) + 1)


lcd = None
touch = None
tracker = None
try:
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.skip_frames(time=1000)
    lcd = display.Display()
    touch = lcd_touch.Touch()
    tracker = espdl.NanoTrack(
        MODEL_DIR + "/template_deploy.espdl",
        MODEL_DIR + "/search_deploy.espdl",
        MODEL_DIR + "/head_deploy.espdl",
    )
    # Keep annotations off the sensor image and the frozen selection image.
    frame = sensor.snapshot()
    width, height = frame.width(), frame.height()
    canvas = frame.copy()
    frozen = frame.copy()
    # Explicit placement uses exactly the same transform as touch mapping.
    scale = min(lcd.width() / width, lcd.height() / height)
    offset_x = (lcd.width() - int(width * scale)) // 2
    offset_y = (lcd.height() - int(height * scale)) // 2
    start = None
    end = None
    tracking = False
    previous_down = False
    lost = 0
    message = "Drag to select a target"

    while True:
        points = touch.read()
        down = bool(points)
        if down and not previous_down:
            point = touch_to_image(points[0], offset_x, offset_y, scale, width, height)
            if point is not None:
                # Freeze the last displayed raw frame so the selected pixels
                # and the template correspond even if the target moves.
                frozen.draw_image(frame, 0, 0)
                start = end = point
                tracking = False
                message = "Release to start tracking"
        if start is not None and down:
            end = touch_to_image(points[0], offset_x, offset_y, scale, width, height, clamp=True)
        if start is not None and not down:
            roi = selection(start, end)
            start = None
            if roi[2] >= MIN_ROI and roi[3] >= MIN_ROI:
                canvas.draw_image(frozen, 0, 0)
                canvas.draw_string(2, 2, "Initializing...", color=(255, 255, 0))
                lcd.write(canvas, x=offset_x, y=offset_y, x_scale=scale, y_scale=scale, fit=False)
                tracker.init(frozen, roi)
                tracking = True
                lost = 0
                message = "Touch to select again"
            else:
                message = "Select at least 10x10 pixels"
        previous_down = down

        if start is not None:
            canvas.draw_image(frozen, 0, 0)
            canvas.draw_rectangle(selection(start, end), color=(255, 255, 0), thickness=2)
        else:
            frame = sensor.snapshot()
            begin = time.ticks_ms()
            result = tracker.track(frame, threshold=0.5) if tracking else None
            canvas.draw_image(frame, 0, 0)
            if result is not None:
                x, y, w, h, score = result
                lost = 0
                canvas.draw_rectangle(x, y, w, h, color=(0, 255, 0), thickness=2)
                canvas.draw_string(2, height - 14, "%.2f  %d ms" % (score, time.ticks_diff(time.ticks_ms(), begin)),
                                   color=(0, 255, 0))
            elif tracking:
                lost += 1
                if lost >= LOST_FRAMES:
                    tracking = False
                    message = "Lost - drag to select again"
        canvas.draw_string(2, 2, message, color=(255, 255, 255))
        lcd.write(canvas, x=offset_x, y=offset_y, x_scale=scale, y_scale=scale, fit=False)
        time.sleep_ms(10)
finally:
    try:
        if tracker is not None:
            tracker.deinit()
    finally:
        try:
            if touch is not None:
                touch.deinit()
        finally:
            if lcd is not None:
                lcd.deinit()
