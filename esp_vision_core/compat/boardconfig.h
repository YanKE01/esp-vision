/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define ESP_VISION_BOARD_ARCH                  "ESP32"
#define ESP_VISION_BOARD_TYPE                  "ESP_VISION_CORE"
#define ESP_VISION_PORT_ESP32                  (1)

#define ESP_VISION_IMLIB_PROFILER_ENABLE       (0)
#define ESP_VISION_IMLIB_GPU_ENABLE            (0)
#define ESP_VISION_IMLIB_JPEG_CODEC_ENABLE     (0)

#define ESP_VISION_CACHE_LINE_SIZE             (32)
#define ESP_VISION_ALLOC_ALIGNMENT             (ESP_VISION_CACHE_LINE_SIZE)
#define ESP_VISION_DMA_ALIGNMENT               (ESP_VISION_CACHE_LINE_SIZE)

#define ESP_VISION_JPEG_QUALITY_LOW            (60)
#define ESP_VISION_JPEG_QUALITY_HIGH           (60)
#define ESP_VISION_JPEG_QUALITY_THRESHOLD      (320 * 240 * 2)
