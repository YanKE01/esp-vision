/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "camera.h"

#include <string.h>

#include "boardconfig.h"

#ifndef CMSIS_MCU_H
#define CMSIS_MCU_H "cmsis_compiler.h"
#endif

#ifndef NO_QSTR
#include "imlib.h"
#endif

typedef struct {
    bool initialized;
    bool hmirror;
    bool vflip;
    uint32_t width;
    uint32_t height;
    pixformat_t pixfmt;
    uint32_t frame_id;
} esp_vision_fake_camera_context_t;

static esp_vision_fake_camera_context_t s_camera;

static void esp_vision_fake_camera_set_defaults(void)
{
    s_camera.width = ESP_VISION_CAMERA_OUTPUT_QVGA_WIDTH;
    s_camera.height = ESP_VISION_CAMERA_OUTPUT_QVGA_HEIGHT;
    s_camera.pixfmt = PIXFORMAT_RGB565;
}

void esp_vision_camera_init0(void)
{
    memset(&s_camera, 0, sizeof(s_camera));
    esp_vision_fake_camera_set_defaults();
}

esp_err_t esp_vision_camera_init(void)
{
    if ((s_camera.width == 0) || (s_camera.height == 0)) {
        esp_vision_fake_camera_set_defaults();
    }

    s_camera.initialized = true;
    return ESP_OK;
}

void esp_vision_camera_deinit(void)
{
    s_camera.initialized = false;
}

bool esp_vision_camera_is_ready(void)
{
    return s_camera.initialized && (s_camera.width != 0) && (s_camera.height != 0);
}

esp_err_t esp_vision_camera_set_pixformat(uint32_t pixfmt)
{
    if ((pixfmt != PIXFORMAT_RGB565) && (pixfmt != PIXFORMAT_GRAYSCALE)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_camera.pixfmt = (pixformat_t)pixfmt;
    return ESP_OK;
}

uint32_t esp_vision_camera_get_pixformat(void)
{
    return s_camera.pixfmt;
}

esp_err_t esp_vision_camera_get_framesize_dimensions(esp_vision_camera_framesize_t framesize,
                                                     uint32_t *width,
                                                     uint32_t *height)
{
    if ((width == NULL) || (height == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (framesize) {
    case ESP_VISION_CAMERA_FRAMESIZE_QQVGA:
        *width = ESP_VISION_CAMERA_OUTPUT_QQVGA_WIDTH;
        *height = ESP_VISION_CAMERA_OUTPUT_QQVGA_HEIGHT;
        return ESP_OK;
    case ESP_VISION_CAMERA_FRAMESIZE_QVGA:
        *width = ESP_VISION_CAMERA_OUTPUT_QVGA_WIDTH;
        *height = ESP_VISION_CAMERA_OUTPUT_QVGA_HEIGHT;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t esp_vision_camera_set_framesize(esp_vision_camera_framesize_t framesize)
{
    uint32_t width = 0;
    uint32_t height = 0;
    esp_err_t ret = esp_vision_camera_get_framesize_dimensions(framesize, &width, &height);
    if (ret != ESP_OK) {
        return ret;
    }

    s_camera.width = width;
    s_camera.height = height;
    return ESP_OK;
}

uint32_t esp_vision_camera_get_width(void)
{
    return s_camera.width;
}

uint32_t esp_vision_camera_get_height(void)
{
    return s_camera.height;
}

esp_err_t esp_vision_camera_set_hmirror(bool enable)
{
    s_camera.hmirror = enable;
    return ESP_OK;
}

bool esp_vision_camera_get_hmirror(void)
{
    return s_camera.hmirror;
}

esp_err_t esp_vision_camera_set_vflip(bool enable)
{
    s_camera.vflip = enable;
    return ESP_OK;
}

bool esp_vision_camera_get_vflip(void)
{
    return s_camera.vflip;
}

uint32_t esp_vision_camera_get_sensor_id(void)
{
    return ESP_VISION_CAMERA_SENSOR_ID;
}

static size_t esp_vision_fake_camera_bpp(uint32_t pixfmt)
{
    switch (pixfmt) {
    case PIXFORMAT_GRAYSCALE:
        return sizeof(uint8_t);
    case PIXFORMAT_RGB565:
        return sizeof(uint16_t);
    default:
        return 0;
    }
}

size_t esp_vision_camera_frame_size(void)
{
    return (size_t)s_camera.width * (size_t)s_camera.height * esp_vision_fake_camera_bpp(s_camera.pixfmt);
}

static uint16_t esp_vision_fake_camera_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((uint16_t)(r & 0xf8) << 8) |
           (uint16_t)((uint16_t)(g & 0xfc) << 3) |
           (uint16_t)(b >> 3);
}

static void esp_vision_fake_camera_source_xy(uint32_t x, uint32_t y, uint32_t *src_x, uint32_t *src_y)
{
    *src_x = s_camera.hmirror ? (s_camera.width - 1U - x) : x;
    *src_y = s_camera.vflip ? (s_camera.height - 1U - y) : y;
}

static void esp_vision_fake_camera_pixel(uint32_t x, uint32_t y, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t src_x = 0;
    uint32_t src_y = 0;
    esp_vision_fake_camera_source_xy(x, y, &src_x, &src_y);

    uint32_t stripe = (src_x / 32U) % 6U;
    uint8_t base_r = 0;
    uint8_t base_g = 0;
    uint8_t base_b = 0;

    switch (stripe) {
    case 0:
        base_r = 220;
        break;
    case 1:
        base_g = 220;
        break;
    case 2:
        base_b = 220;
        break;
    case 3:
        base_r = 220;
        base_g = 220;
        break;
    case 4:
        base_g = 220;
        base_b = 220;
        break;
    default:
        base_r = 220;
        base_b = 220;
        break;
    }

    uint8_t x_grad = (uint8_t)((src_x * 255U) / (s_camera.width - 1U));
    uint8_t y_grad = (uint8_t)((src_y * 255U) / (s_camera.height - 1U));
    uint8_t frame = (uint8_t)(s_camera.frame_id * 7U);

    *r = (uint8_t)((base_r / 2U) + (x_grad / 2U));
    *g = (uint8_t)((base_g / 2U) + (y_grad / 2U));
    *b = (uint8_t)((base_b / 2U) + (frame / 2U));

    if ((src_x == src_y) || ((src_x + src_y) == (s_camera.width - 1U))) {
        *r = 255;
        *g = 255;
        *b = 255;
    }
}

esp_err_t esp_vision_camera_capture(uint8_t *pixels, size_t pixels_size)
{
    if ((pixels == NULL) || !esp_vision_camera_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t expected_size = esp_vision_camera_frame_size();
    if ((expected_size == 0) || (pixels_size < expected_size)) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (uint32_t y = 0; y < s_camera.height; y++) {
        for (uint32_t x = 0; x < s_camera.width; x++) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            esp_vision_fake_camera_pixel(x, y, &r, &g, &b);

            size_t offset = ((size_t)y * (size_t)s_camera.width) + (size_t)x;
            if (s_camera.pixfmt == PIXFORMAT_GRAYSCALE) {
                pixels[offset] = (uint8_t)(((uint32_t)r + (uint32_t)g + (uint32_t)b) / 3U);
            } else {
                uint16_t rgb565 = esp_vision_fake_camera_rgb565(r, g, b);
                pixels[offset * 2U] = (uint8_t)(rgb565 & 0xff);
                pixels[(offset * 2U) + 1U] = (uint8_t)(rgb565 >> 8);
            }
        }
    }

    s_camera.frame_id++;
    return ESP_OK;
}

esp_err_t esp_vision_camera_snapshot(image_t *img, uint8_t *pixels, size_t pixels_size)
{
    if (img == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_vision_camera_capture(pixels, pixels_size);
    if (ret != ESP_OK) {
        return ret;
    }

    img->w = (int32_t)s_camera.width;
    img->h = (int32_t)s_camera.height;
    img->pixfmt = s_camera.pixfmt;
    img->size = 0;
    img->_raw = NULL;
    img->pixels = pixels;
    return ESP_OK;
}

esp_err_t esp_vision_camera_get_status(esp_vision_camera_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    status->ready = esp_vision_camera_is_ready();
    status->sensor_id = ESP_VISION_CAMERA_SENSOR_ID;
    status->raw_input_width = s_camera.width;
    status->raw_input_height = s_camera.height;
    status->active_input_width = s_camera.width;
    status->active_input_height = s_camera.height;
    status->active_input_offset_x = 0;
    status->active_input_offset_y = 0;
    status->width = s_camera.width;
    status->height = s_camera.height;
    status->pixfmt = s_camera.pixfmt;
    status->hmirror = s_camera.hmirror;
    status->vflip = s_camera.vflip;
    return ESP_OK;
}
