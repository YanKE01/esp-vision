/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_vision_core.h"

#include <string.h>

#include "sdkconfig.h"

#ifndef CMSIS_MCU_H
#define CMSIS_MCU_H "cmsis_compiler.h"
#endif

#include "fb_alloc.h"
#include "imlib.h"

static pixformat_t esp_vision_to_imlib_pixformat(esp_vision_pixformat_t pixformat)
{
    switch (pixformat) {
    case ESP_VISION_PIXFORMAT_BINARY:
        return PIXFORMAT_BINARY;
    case ESP_VISION_PIXFORMAT_GRAYSCALE:
        return PIXFORMAT_GRAYSCALE;
    case ESP_VISION_PIXFORMAT_RGB565:
        return PIXFORMAT_RGB565;
    default:
        return PIXFORMAT_INVALID;
    }
}

static esp_err_t esp_vision_image_to_imlib(const esp_vision_image_t *image, image_t *out)
{
    if ((image == NULL) || (out == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t expected_size = 0;
    esp_err_t ret = esp_vision_image_size(image->width, image->height, image->pixformat, &expected_size);
    if (ret != ESP_OK) {
        return ret;
    }
    if ((image->data == NULL) || (image->size < expected_size)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->w = image->width;
    out->h = image->height;
    out->pixfmt = esp_vision_to_imlib_pixformat(image->pixformat);
    out->size = image->size;
    out->data = image->data;
    return ESP_OK;
}

static esp_err_t esp_vision_roi_to_imlib(const esp_vision_image_t *image,
                                         const esp_vision_rect_t *roi,
                                         rectangle_t *out)
{
    if ((image == NULL) || (out == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (roi == NULL) {
        out->x = 0;
        out->y = 0;
        out->w = image->width;
        out->h = image->height;
        return ESP_OK;
    }

    if ((roi->x < 0) || (roi->y < 0) || (roi->w <= 0) || (roi->h <= 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (((int32_t)roi->x + roi->w > image->width) ||
            ((int32_t)roi->y + roi->h > image->height)) {
        return ESP_ERR_INVALID_ARG;
    }

    out->x = roi->x;
    out->y = roi->y;
    out->w = roi->w;
    out->h = roi->h;
    return ESP_OK;
}

static esp_vision_rect_t esp_vision_rect_from_imlib(const rectangle_t *rect)
{
    esp_vision_rect_t out = {
        .x = rect->x,
        .y = rect->y,
        .w = rect->w,
        .h = rect->h,
    };
    return out;
}

static esp_vision_point_t esp_vision_point_from_imlib(const point_t *point)
{
    esp_vision_point_t out = {
        .x = point->x,
        .y = point->y,
    };
    return out;
}

static void esp_vision_thresholds_to_imlib(const esp_vision_find_blobs_config_t *config, list_t *thresholds)
{
    list_init(thresholds, sizeof(color_thresholds_list_lnk_data_t));

    if ((config == NULL) || (config->thresholds == NULL)) {
        return;
    }

    for (size_t i = 0; i < config->threshold_count; i++) {
        const esp_vision_color_threshold_t *src = &config->thresholds[i];
        color_thresholds_list_lnk_data_t dst = {
            .LMin = src->l_min,
            .LMax = src->l_max,
            .AMin = src->a_min,
            .AMax = src->a_max,
            .BMin = src->b_min,
            .BMax = src->b_max,
        };
        list_push_back(thresholds, &dst);
    }
}

void esp_vision_core_init(void)
{
    fb_alloc_init0();
    imlib_init();
}

void esp_vision_core_deinit(void)
{
    imlib_deinit();
    fb_alloc_init0();
}

esp_err_t esp_vision_image_size(int32_t width,
                                int32_t height,
                                esp_vision_pixformat_t pixformat,
                                size_t *size)
{
    if ((width <= 0) || (height <= 0) || (size == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t w = (size_t)width;
    const size_t h = (size_t)height;

    switch (pixformat) {
    case ESP_VISION_PIXFORMAT_BINARY: {
        if (w > (SIZE_MAX - 31U)) {
            return ESP_ERR_INVALID_ARG;
        }
        size_t row_words = (w + 31U) / 32U;
        if ((row_words > (SIZE_MAX / sizeof(uint32_t))) ||
                ((row_words * sizeof(uint32_t)) > (SIZE_MAX / h))) {
            return ESP_ERR_INVALID_ARG;
        }
        *size = row_words * sizeof(uint32_t) * h;
        return ESP_OK;
    }
    case ESP_VISION_PIXFORMAT_GRAYSCALE:
        if (w > (SIZE_MAX / h)) {
            return ESP_ERR_INVALID_ARG;
        }
        *size = w * h;
        return ESP_OK;
    case ESP_VISION_PIXFORMAT_RGB565:
        if ((w > (SIZE_MAX / h)) || ((w * h) > (SIZE_MAX / sizeof(uint16_t)))) {
            return ESP_ERR_INVALID_ARG;
        }
        *size = w * h * sizeof(uint16_t);
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t esp_vision_image_validate(const esp_vision_image_t *image)
{
    image_t unused;
    return esp_vision_image_to_imlib(image, &unused);
}

esp_err_t esp_vision_image_draw_line(esp_vision_image_t *image,
                                     int x0,
                                     int y0,
                                     int x1,
                                     int y1,
                                     int color,
                                     int thickness)
{
#if CONFIG_ESP_VISION_CORE_ENABLE_DRAWING
    image_t img;
    esp_err_t ret = esp_vision_image_to_imlib(image, &img);
    if (ret != ESP_OK) {
        return ret;
    }

    imlib_draw_line(&img, x0, y0, x1, y1, color, thickness);
    return ESP_OK;
#else
    (void)image;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)color;
    (void)thickness;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_vision_image_draw_rectangle(esp_vision_image_t *image,
                                          const esp_vision_rect_t *rect,
                                          int color,
                                          int thickness,
                                          bool fill)
{
#if CONFIG_ESP_VISION_CORE_ENABLE_DRAWING
    if (rect == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    image_t img;
    esp_err_t ret = esp_vision_image_to_imlib(image, &img);
    if (ret != ESP_OK) {
        return ret;
    }

    imlib_draw_rectangle(&img, rect->x, rect->y, rect->w, rect->h, color, thickness, fill);
    return ESP_OK;
#else
    (void)image;
    (void)rect;
    (void)color;
    (void)thickness;
    (void)fill;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_vision_image_draw_circle(esp_vision_image_t *image,
                                       int cx,
                                       int cy,
                                       int radius,
                                       int color,
                                       int thickness,
                                       bool fill)
{
#if CONFIG_ESP_VISION_CORE_ENABLE_DRAWING
    image_t img;
    esp_err_t ret = esp_vision_image_to_imlib(image, &img);
    if (ret != ESP_OK) {
        return ret;
    }

    imlib_draw_circle(&img, cx, cy, radius, color, thickness, fill);
    return ESP_OK;
#else
    (void)image;
    (void)cx;
    (void)cy;
    (void)radius;
    (void)color;
    (void)thickness;
    (void)fill;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_vision_image_find_blobs(const esp_vision_image_t *image,
                                      const esp_vision_rect_t *roi,
                                      const esp_vision_find_blobs_config_t *config,
                                      esp_vision_blob_t *results,
                                      size_t result_capacity,
                                      size_t *result_count)
{
#if CONFIG_ESP_VISION_CORE_ENABLE_BLOBS
    if ((config == NULL) || (result_count == NULL) ||
            ((result_capacity > 0) && (results == NULL))) {
        return ESP_ERR_INVALID_ARG;
    }

    image_t img;
    rectangle_t imlib_roi;
    esp_err_t ret = esp_vision_image_to_imlib(image, &img);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_vision_roi_to_imlib(image, roi, &imlib_roi);
    if (ret != ESP_OK) {
        return ret;
    }

    unsigned int x_stride = config->x_stride ? config->x_stride : 2;
    unsigned int y_stride = config->y_stride ? config->y_stride : 1;
    unsigned int area_threshold = config->area_threshold ? config->area_threshold : 10;
    unsigned int pixels_threshold = config->pixels_threshold ? config->pixels_threshold : 10;

    list_t thresholds;
    esp_vision_thresholds_to_imlib(config, &thresholds);
    if (!list_size(&thresholds)) {
        *result_count = 0;
        return ESP_OK;
    }

    list_t out;
    fb_alloc_mark();
    imlib_find_blobs(&out,
                     &img,
                     &imlib_roi,
                     x_stride,
                     y_stride,
                     &thresholds,
                     config->invert,
                     area_threshold,
                     pixels_threshold,
                     config->merge,
                     config->margin,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     0,
                     0);
    fb_alloc_free_till_mark();
    list_free(&thresholds);

    size_t total = 0;
    esp_err_t status = ESP_OK;
    while (list_size(&out)) {
        find_blobs_list_lnk_data_t blob;
        list_pop_front(&out, &blob);
        if (total < result_capacity) {
            results[total].rect = esp_vision_rect_from_imlib(&blob.rect);
            results[total].pixels = blob.pixels;
            results[total].perimeter = blob.perimeter;
            results[total].code = blob.code;
            results[total].count = blob.count;
            results[total].centroid_x = blob.centroid_x;
            results[total].centroid_y = blob.centroid_y;
            results[total].rotation = blob.rotation;
            results[total].roundness = blob.roundness;
        } else {
            status = ESP_ERR_INVALID_SIZE;
        }
        if (blob.x_hist_bins != NULL) {
            m_free(blob.x_hist_bins);
        }
        if (blob.y_hist_bins != NULL) {
            m_free(blob.y_hist_bins);
        }
        total++;
    }
    *result_count = total;
    return status;
#else
    (void)image;
    (void)roi;
    (void)config;
    (void)results;
    (void)result_capacity;
    if (result_count != NULL) {
        *result_count = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_vision_image_find_qrcodes(const esp_vision_image_t *image,
                                        const esp_vision_rect_t *roi,
                                        esp_vision_qrcode_t *results,
                                        size_t result_capacity,
                                        size_t *result_count)
{
#if CONFIG_ESP_VISION_CORE_ENABLE_QRCODES
    if ((result_count == NULL) || ((result_capacity > 0) && (results == NULL))) {
        return ESP_ERR_INVALID_ARG;
    }

    image_t img;
    rectangle_t imlib_roi;
    esp_err_t ret = esp_vision_image_to_imlib(image, &img);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_vision_roi_to_imlib(image, roi, &imlib_roi);
    if (ret != ESP_OK) {
        return ret;
    }

    list_t out;
    fb_alloc_mark();
    imlib_find_qrcodes(&out, &img, &imlib_roi);
    fb_alloc_free_till_mark();

    size_t total = 0;
    esp_err_t status = ESP_OK;
    while (list_size(&out)) {
        find_qrcodes_list_lnk_data_t qrcode;
        list_pop_front(&out, &qrcode);
        if (total < result_capacity) {
            esp_vision_qrcode_t *dst = &results[total];
            for (size_t i = 0; i < 4; i++) {
                dst->corners[i] = esp_vision_point_from_imlib(&qrcode.corners[i]);
            }
            dst->rect = esp_vision_rect_from_imlib(&qrcode.rect);
            dst->payload_len = qrcode.payload_len;
            dst->version = qrcode.version;
            dst->ecc_level = qrcode.ecc_level;
            dst->mask = qrcode.mask;
            dst->data_type = qrcode.data_type;
            dst->eci = qrcode.eci;
            if ((dst->payload != NULL) && (dst->payload_size > 0)) {
                size_t copy_len = qrcode.payload_len;
                if (copy_len >= dst->payload_size) {
                    copy_len = dst->payload_size - 1;
                    status = ESP_ERR_INVALID_SIZE;
                }
                memcpy(dst->payload, qrcode.payload, copy_len);
                dst->payload[copy_len] = '\0';
            }
        } else {
            status = ESP_ERR_INVALID_SIZE;
        }
        m_free(qrcode.payload);
        total++;
    }
    *result_count = total;
    return status;
#else
    (void)image;
    (void)roi;
    (void)results;
    (void)result_capacity;
    if (result_count != NULL) {
        *result_count = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_vision_image_find_apriltags(const esp_vision_image_t *image,
                                          const esp_vision_rect_t *roi,
                                          const esp_vision_find_apriltags_config_t *config,
                                          esp_vision_apriltag_t *results,
                                          size_t result_capacity,
                                          size_t *result_count)
{
#if CONFIG_ESP_VISION_CORE_ENABLE_APRILTAGS
    if ((result_count == NULL) || ((result_capacity > 0) && (results == NULL))) {
        return ESP_ERR_INVALID_ARG;
    }

    image_t img;
    rectangle_t imlib_roi;
    esp_err_t ret = esp_vision_image_to_imlib(image, &img);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_vision_roi_to_imlib(image, roi, &imlib_roi);
    if (ret != ESP_OK) {
        return ret;
    }

    if ((imlib_roi.w < 4) || (imlib_roi.h < 4)) {
        *result_count = 0;
        return ESP_OK;
    }
#ifndef IMLIB_ENABLE_HIGH_RES_APRILTAGS
    if (((int32_t)imlib_roi.w * imlib_roi.h) >= 65536) {
        return ESP_ERR_INVALID_SIZE;
    }
#endif

    esp_vision_find_apriltags_config_t default_config = {
        .families = ESP_VISION_APRILTAG_TAG36H11,
        .fx = (2.8f / 3.984f) * image->width,
        .fy = (2.8f / 2.952f) * image->height,
        .cx = image->width * 0.5f,
        .cy = image->height * 0.5f,
        .pose = true,
    };
    if (config != NULL) {
        default_config = *config;
        if (default_config.families == 0) {
            default_config.families = ESP_VISION_APRILTAG_TAG36H11;
        }
        if (default_config.fx == 0.0f) {
            default_config.fx = (2.8f / 3.984f) * image->width;
        }
        if (default_config.fy == 0.0f) {
            default_config.fy = (2.8f / 2.952f) * image->height;
        }
        if (default_config.cx == 0.0f) {
            default_config.cx = image->width * 0.5f;
        }
        if (default_config.cy == 0.0f) {
            default_config.cy = image->height * 0.5f;
        }
    }

    list_t out;
    fb_alloc_mark();
    imlib_find_apriltags(&out,
                         &img,
                         &imlib_roi,
                         (apriltag_families_t)default_config.families,
                         default_config.fx,
                         default_config.fy,
                         default_config.cx,
                         default_config.cy,
                         default_config.pose);
    fb_alloc_free_till_mark();

    size_t total = 0;
    esp_err_t status = ESP_OK;
    while (list_size(&out)) {
        find_apriltags_list_lnk_data_t tag;
        list_pop_front(&out, &tag);
        if (total < result_capacity) {
            esp_vision_apriltag_t *dst = &results[total];
            for (size_t i = 0; i < 4; i++) {
                dst->corners[i] = esp_vision_point_from_imlib(&tag.corners[i]);
            }
            dst->rect = esp_vision_rect_from_imlib(&tag.rect);
            dst->id = tag.id;
            dst->family = tag.family;
            dst->hamming = tag.hamming;
            dst->centroid_x = tag.centroid_x;
            dst->centroid_y = tag.centroid_y;
            dst->goodness = tag.goodness;
            dst->decision_margin = tag.decision_margin;
            dst->x_translation = tag.x_translation;
            dst->y_translation = tag.y_translation;
            dst->z_translation = tag.z_translation;
            dst->x_rotation = tag.x_rotation;
            dst->y_rotation = tag.y_rotation;
            dst->z_rotation = tag.z_rotation;
        } else {
            status = ESP_ERR_INVALID_SIZE;
        }
        total++;
    }
    *result_count = total;
    return status;
#else
    (void)image;
    (void)roi;
    (void)config;
    (void)results;
    (void)result_capacity;
    if (result_count != NULL) {
        *result_count = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
