/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP_VISION_BOARD_CONFIG_H
#define ESP_VISION_BOARD_CONFIG_H

#define ESP_VISION_BOARD_ARCH                  "ESP32C5"
#define ESP_VISION_BOARD_TYPE                  "ESP32_C5_DEVKITC_1_N8R8"
#define ESP_VISION_PORT_ESP32                       (1)

#define ESP_VISION_IMLIB_PROFILER_ENABLE            (0)
#define ESP_VISION_IMLIB_GPU_ENABLE                 (0)
#define ESP_VISION_IMLIB_JPEG_CODEC_ENABLE          (0)

#define ESP_VISION_CACHE_LINE_SIZE                  (32)
#define ESP_VISION_ALLOC_ALIGNMENT                  (ESP_VISION_CACHE_LINE_SIZE)
#define ESP_VISION_DMA_ALIGNMENT                    (ESP_VISION_CACHE_LINE_SIZE)

#define ESP_VISION_JPEG_QUALITY_LOW                 (60)
#define ESP_VISION_JPEG_QUALITY_HIGH                (60)
#define ESP_VISION_JPEG_QUALITY_THRESHOLD           (320 * 240 * 2)

/* Generated test camera configuration. */
#define ESP_VISION_CAMERA_SENSOR_ID                 (0xc500)
#define ESP_VISION_CAMERA_OUTPUT_QQVGA_WIDTH        (160)
#define ESP_VISION_CAMERA_OUTPUT_QQVGA_HEIGHT       (120)
#define ESP_VISION_CAMERA_OUTPUT_QVGA_WIDTH         (320)
#define ESP_VISION_CAMERA_OUTPUT_QVGA_HEIGHT        (240)

#endif /* ESP_VISION_BOARD_CONFIG_H */
