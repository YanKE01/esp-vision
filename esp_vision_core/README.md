# ESP-VISION Core

`esp-vision` is the reusable ESP-IDF image processing component published from the ESP-VISION repository root.

It builds the public core API together with the required `imlib` sources. It does not initialize cameras, displays, storage, USB, RTSP, or MicroPython. Callers provide image buffers and receive processed pixels or detection results.

Feature participation is controlled by the `ESP_VISION_CORE_*` Kconfig options.
