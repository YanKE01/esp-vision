lcd_touch -- LCD 触摸输入
========================

:link_to_translation:`en:[English]`

``lcd_touch.Touch`` 使用板卡的 Board Manager ``lcd_touch`` 设备。ESP32-P4 Function EV Board 和 ESP32-S31 Mosaico 提供触摸配置；其他板卡若没有可用控制器，构造 ``Touch`` 时会抛出 ``OSError``。

.. code-block:: python

   import lcd_touch

   touch = lcd_touch.Touch()
   try:
       for x, y in touch.read():
           print(x, y)
   finally:
       touch.deinit()

``read()`` 返回最多五个 LCD 物理坐标点，松手时返回空元组。显示相机图像时，触摸点需要通过相同的偏移和缩放映射到图像坐标；留黑区域不属于图像。只有最近创建的 ``Touch`` 对象拥有板载设备。

示例：``example/06-Peripherals/03-Touch/touch_lcd.py`` 和 ``example/03-Machine-Learning/00-ESP-DL/nanotrack_touch.py``。

.. include:: _generated/lcd_touch.rst
