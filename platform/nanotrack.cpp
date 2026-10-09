/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nanotrack.hpp"

#include <cstring>
#include <initializer_list>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "dl_model_base.hpp"

// The MicroPython QSTR preprocessing pass does not inherit all imlib defines.
#ifndef CMSIS_MCU_H
#define CMSIS_MCU_H "cmsis_compiler.h"
#endif
extern "C" {
#include "imlib.h"
}

namespace esp_vision {

// Only P4/S3 have SIMD on both cores. Keep S31 inference on the caller's core.
#if CONFIG_IDF_TARGET_ESP32S31
static constexpr dl::runtime_mode_t s_runtime_mode = dl::RUNTIME_MODE_SINGLE_CORE;
#else
static constexpr dl::runtime_mode_t s_runtime_mode = dl::RUNTIME_MODE_MULTI_CORE;
#endif

#if CONFIG_IDF_TARGET_ESP32S3
static constexpr bool s_half_up = true;
#else
static constexpr bool s_half_up = false;
#endif

static bool tensor_matches(dl::TensorBase *tensor, std::initializer_list<int> shape)
{
    return tensor && tensor->get_element_ptr() && tensor->dtype == dl::DATA_TYPE_INT8
           && !tensor->exponent.is_per_channel()
           && tensor->get_exponent() >= -30 && tensor->get_exponent() <= 30
           && tensor->shape.size() == shape.size()
           && std::equal(shape.begin(), shape.end(), tensor->shape.begin());
}

static bool valid_image(const image_t *image)
{
    return image && image->pixels && image->w >= 10 && image->h >= 10
           && (image->pixfmt == PIXFORMAT_RGB565 || image->pixfmt == PIXFORMAT_GRAYSCALE);
}

static void copy_feature(dl::TensorBase *source, int8_t *destination, int exponent)
{
    const int8_t *input = source->get_element_ptr<int8_t>();
    float scale = std::ldexp(1.0f, source->get_exponent() - exponent);
    for (int i = 0; i < source->get_size(); ++i) {
        destination[i] = nanotrack::quantize(input[i], scale, s_half_up);
    }
}

NanoTrack::NanoTrack(dl::Model *search, dl::Model *head) : m_search(search), m_head(head)
{
    m_search_input = search->get_input("input");
    m_search_output = search->get_output("output");
    m_head_template = head->get_input("input1");
    m_head_search = head->get_input("input2");
    m_cls = head->get_output("output1");
    m_loc = head->get_output("output2");
    m_valid = search->get_inputs().size() == 1 && search->get_outputs().size() == 1
              && head->get_inputs().size() == 2 && head->get_outputs().size() == 2
              && tensor_matches(m_search_input, {1, 255, 255, 3})
              && tensor_matches(m_search_output, {1, 16, 16, 48})
              && tensor_matches(m_head_template, {1, 8, 8, 48})
              && tensor_matches(m_head_search, {1, 16, 16, 48})
              && tensor_matches(m_cls, {1, 16, 16, 2})
              && tensor_matches(m_loc, {1, 16, 16, 4});
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            m_window[y * 16 + x] = 0.25f * (1 - std::cos(2 * 3.14159265358979323846f * x / 15))
                                   * (1 - std::cos(2 * 3.14159265358979323846f * y / 15));
        }
    }
}

void NanoTrack::pixel(const image_t *image, int x, int y, float *bgr) const
{
    if (x < 0 || y < 0 || x >= image->w || y >= image->h) {
        std::memcpy(bgr, m_padding, sizeof(m_padding));
    } else if (image->pixfmt == PIXFORMAT_GRAYSCALE) {
        bgr[0] = bgr[1] = bgr[2] = image->pixels[y * image->w + x];
    } else {
        uint16_t rgb = reinterpret_cast<const uint16_t *>(image->pixels)[y * image->w + x];
        bgr[0] = COLOR_RGB565_TO_B8(rgb);
        bgr[1] = COLOR_RGB565_TO_G8(rgb);
        bgr[2] = COLOR_RGB565_TO_R8(rgb);
    }
}

// Sample directly into the NHWC INT8 input, avoiding per-frame crop allocations.
void NanoTrack::preprocess(const image_t *image, dl::TensorBase *input, int size, float side_float) const
{
    int side = std::max(2, static_cast<int>(std::nearbyint(side_float)));
    int origin_x = static_cast<int>(std::floor(m_state.cx - (side + 1) * 0.5f + 0.5f));
    int origin_y = static_cast<int>(std::floor(m_state.cy - (side + 1) * 0.5f + 0.5f));
    float step = static_cast<float>(side) / size;
    float inverse_scale = std::ldexp(1.0f, -input->get_exponent());
    int8_t *output = input->get_element_ptr<int8_t>();
    for (int y = 0; y < size; ++y) {
        float sy = std::max(0.0f, std::min(static_cast<float>(side - 1), (y + 0.5f) * step - 0.5f));
        int y0 = static_cast<int>(sy);
        int y1 = std::min(y0 + 1, side - 1);
        float fy = sy - y0;
        for (int x = 0; x < size; ++x) {
            float sx = std::max(0.0f, std::min(static_cast<float>(side - 1), (x + 0.5f) * step - 0.5f));
            int x0 = static_cast<int>(sx);
            int x1 = std::min(x0 + 1, side - 1);
            float fx = sx - x0;
            float a[3], b[3], c[3], d[3];
            pixel(image, origin_x + x0, origin_y + y0, a);
            pixel(image, origin_x + x1, origin_y + y0, b);
            pixel(image, origin_x + x0, origin_y + y1, c);
            pixel(image, origin_x + x1, origin_y + y1, d);
            for (int channel = 0; channel < 3; ++channel) {
                float value = ((1 - fx) * a[channel] + fx * b[channel]) * (1 - fy)
                              + ((1 - fx) * c[channel] + fx * d[channel]) * fy;
                // The PC pipeline resizes a uint8 image before quantization.
                *output++ = nanotrack::quantize(std::nearbyint(value), inverse_scale, s_half_up);
            }
        }
    }
}

esp_err_t NanoTrack::init(const image_t *image, const rectangle_t &roi, dl::Model *model)
{
    m_ready = false;
    if (!m_valid || !valid_image(image) || roi.x < 0 || roi.y < 0 || roi.w < 10 || roi.h < 10
            || roi.x + roi.w > image->w || roi.y + roi.h > image->h) {
        return ESP_ERR_INVALID_ARG;
    }
    dl::TensorBase *input = model->get_input("input");
    dl::TensorBase *output = model->get_output("output");
    if (model->get_inputs().size() != 1 || model->get_outputs().size() != 1
            || !tensor_matches(input, {1, 127, 127, 3}) || !tensor_matches(output, {1, 8, 8, 48})) {
        return ESP_ERR_INVALID_ARG;
    }
    m_width = image->w;
    m_height = image->h;
    m_state = {roi.x + (roi.w - 1) * 0.5f, roi.y + (roi.h - 1) * 0.5f,
               static_cast<float>(roi.w), static_cast<float>(roi.h)
              };
    double sum[3] = {};
    for (int y = 0; y < image->h; ++y) {
        for (int x = 0; x < image->w; ++x) {
            float bgr[3];
            pixel(image, x, y, bgr);
            for (int c = 0; c < 3; ++c) {
                sum[c] += bgr[c];
            }
        }
    }
    for (int c = 0; c < 3; ++c) {
        m_padding[c] = std::nearbyint(sum[c] / (image->w * image->h));
    }
    preprocess(image, input, 127, nanotrack::extent(m_state.w, m_state.h));
    model->run(s_runtime_mode);
    copy_feature(output, m_template, m_head_template->get_exponent());
    m_ready = true;
    return ESP_OK;
}

esp_err_t NanoTrack::track(const image_t *image, float threshold, nanotrack::Box &box, bool &found)
{
    found = false;
    if (!m_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!valid_image(image) || image->w != m_width || image->h != m_height
            || !std::isfinite(threshold) || threshold < 0 || threshold > 1) {
        return ESP_ERR_INVALID_ARG;
    }
    preprocess(image, m_search_input, 255, nanotrack::extent(m_state.w, m_state.h) * 255 / 127);
    m_search->run(s_runtime_mode);
    // Inputs may share the execution arena with intermediate tensors; restore
    // the cached template before every head invocation.
    std::memcpy(m_head_template->get_element_ptr(), m_template, sizeof(m_template));
    copy_feature(m_search_output, m_head_search->get_element_ptr<int8_t>(), m_head_search->get_exponent());
    m_head->run(s_runtime_mode);
    found = nanotrack::decode(m_cls->get_element_ptr<int8_t>(), std::ldexp(1.0f, m_cls->get_exponent()),
                              m_loc->get_element_ptr<int8_t>(), std::ldexp(1.0f, m_loc->get_exponent()),
                              m_window, m_width, m_height, threshold, m_state, box);
    return ESP_OK;
}

} // namespace esp_vision
