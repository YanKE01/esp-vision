/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_VISION_PIXFORMAT_BINARY = 0,
    ESP_VISION_PIXFORMAT_GRAYSCALE,
    ESP_VISION_PIXFORMAT_RGB565,
} esp_vision_pixformat_t;

typedef struct {
    int16_t x;
    int16_t y;
} esp_vision_point_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} esp_vision_rect_t;

typedef struct {
    int32_t width;
    int32_t height;
    esp_vision_pixformat_t pixformat;
    uint8_t *data;
    size_t size;
} esp_vision_image_t;

typedef struct {
    uint8_t l_min;
    uint8_t l_max;
    int8_t a_min;
    int8_t a_max;
    int8_t b_min;
    int8_t b_max;
} esp_vision_color_threshold_t;

typedef struct {
    esp_vision_rect_t rect;
    uint32_t pixels;
    uint32_t perimeter;
    uint32_t code;
    uint32_t count;
    float centroid_x;
    float centroid_y;
    float rotation;
    float roundness;
} esp_vision_blob_t;

typedef struct {
    esp_vision_point_t corners[4];
    esp_vision_rect_t rect;
    char *payload;
    size_t payload_size;
    size_t payload_len;
    uint8_t version;
    uint8_t ecc_level;
    uint8_t mask;
    uint8_t data_type;
    uint32_t eci;
} esp_vision_qrcode_t;

typedef enum {
    ESP_VISION_APRILTAG_TAG16H5 = 1,
    ESP_VISION_APRILTAG_TAG25H7 = 2,
    ESP_VISION_APRILTAG_TAG25H9 = 4,
    ESP_VISION_APRILTAG_TAG36H10 = 8,
    ESP_VISION_APRILTAG_TAG36H11 = 16,
    ESP_VISION_APRILTAG_ARTOOLKIT = 32,
} esp_vision_apriltag_family_t;

typedef struct {
    esp_vision_point_t corners[4];
    esp_vision_rect_t rect;
    uint16_t id;
    uint8_t family;
    uint8_t hamming;
    float centroid_x;
    float centroid_y;
    float goodness;
    float decision_margin;
    float x_translation;
    float y_translation;
    float z_translation;
    float x_rotation;
    float y_rotation;
    float z_rotation;
} esp_vision_apriltag_t;

typedef struct {
    const esp_vision_color_threshold_t *thresholds;
    size_t threshold_count;
    bool invert;
    unsigned int x_stride;
    unsigned int y_stride;
    unsigned int area_threshold;
    unsigned int pixels_threshold;
    bool merge;
    int margin;
} esp_vision_find_blobs_config_t;

typedef struct {
    uint32_t families;
    float fx;
    float fy;
    float cx;
    float cy;
    bool pose;
} esp_vision_find_apriltags_config_t;

void esp_vision_core_init(void);
void esp_vision_core_deinit(void);

esp_err_t esp_vision_image_size(int32_t width,
                                int32_t height,
                                esp_vision_pixformat_t pixformat,
                                size_t *size);

esp_err_t esp_vision_image_validate(const esp_vision_image_t *image);

esp_err_t esp_vision_image_draw_line(esp_vision_image_t *image,
                                     int x0,
                                     int y0,
                                     int x1,
                                     int y1,
                                     int color,
                                     int thickness);
esp_err_t esp_vision_image_draw_rectangle(esp_vision_image_t *image,
                                          const esp_vision_rect_t *rect,
                                          int color,
                                          int thickness,
                                          bool fill);
esp_err_t esp_vision_image_draw_circle(esp_vision_image_t *image,
                                       int cx,
                                       int cy,
                                       int radius,
                                       int color,
                                       int thickness,
                                       bool fill);

esp_err_t esp_vision_image_find_blobs(const esp_vision_image_t *image,
                                      const esp_vision_rect_t *roi,
                                      const esp_vision_find_blobs_config_t *config,
                                      esp_vision_blob_t *results,
                                      size_t result_capacity,
                                      size_t *result_count);

esp_err_t esp_vision_image_find_qrcodes(const esp_vision_image_t *image,
                                        const esp_vision_rect_t *roi,
                                        esp_vision_qrcode_t *results,
                                        size_t result_capacity,
                                        size_t *result_count);

esp_err_t esp_vision_image_find_apriltags(const esp_vision_image_t *image,
                                          const esp_vision_rect_t *roi,
                                          const esp_vision_find_apriltags_config_t *config,
                                          esp_vision_apriltag_t *results,
                                          size_t result_capacity,
                                          size_t *result_count);

#ifdef __cplusplus
}
#endif
