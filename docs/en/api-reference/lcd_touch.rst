lcd_touch -- LCD Touch Input
===========================

:link_to_translation:`zh_CN:[中文]`

``lcd_touch.Touch`` uses the board's Board Manager ``lcd_touch`` device. ESP32-P4 Function EV Board and ESP32-S31 Mosaico provide touch configuration. Other boards raise ``OSError`` when constructing ``Touch`` if a controller is unavailable.

.. code-block:: python

   import lcd_touch

   touch = lcd_touch.Touch()
   try:
       for x, y in touch.read():
           print(x, y)
   finally:
       touch.deinit()

``read()`` returns up to five points in physical LCD coordinates, or an empty tuple when released. Map points through the same offset and scale used to display the camera image; touches in letterbox margins are outside the image. Only the most recently created ``Touch`` object owns the board device.

Examples: ``example/06-Peripherals/03-Touch/touch_lcd.py`` and ``example/03-Machine-Learning/00-ESP-DL/nanotrack_touch.py``.

.. include:: _generated/lcd_touch.rst
