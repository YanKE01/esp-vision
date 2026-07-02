/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_err.h"
#include "esp_vision_core.h"
#include "unity.h"

static void test_image_size_calculation(void)
{
    size_t size = 0;

    TEST_ASSERT_EQUAL(ESP_OK, esp_vision_image_size(33, 2, ESP_VISION_PIXFORMAT_BINARY, &size));
    TEST_ASSERT_EQUAL(sizeof(uint32_t) * 2 * 2, size);

    TEST_ASSERT_EQUAL(ESP_OK, esp_vision_image_size(16, 16, ESP_VISION_PIXFORMAT_GRAYSCALE, &size));
    TEST_ASSERT_EQUAL(16 * 16, size);

    TEST_ASSERT_EQUAL(ESP_OK, esp_vision_image_size(16, 16, ESP_VISION_PIXFORMAT_RGB565, &size));
    TEST_ASSERT_EQUAL(16 * 16 * sizeof(uint16_t), size);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_vision_image_size(0, 16, ESP_VISION_PIXFORMAT_GRAYSCALE, &size));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_vision_image_size(16, 16, 99, &size));
}

static void test_validate_and_draw_grayscale_line(void)
{
    uint8_t framebuffer[16 * 16] = { 0 };
    esp_vision_image_t image = {
        .width = 16,
        .height = 16,
        .pixformat = ESP_VISION_PIXFORMAT_GRAYSCALE,
        .data = framebuffer,
        .size = sizeof(framebuffer),
    };

    TEST_ASSERT_EQUAL(ESP_OK, esp_vision_image_validate(&image));
    TEST_ASSERT_EQUAL(ESP_OK, esp_vision_image_draw_line(&image, 0, 0, 15, 15, 255, 1));
    TEST_ASSERT_EQUAL_UINT8(255, framebuffer[0]);
    TEST_ASSERT_EQUAL_UINT8(255, framebuffer[(15 * 16) + 15]);

    image.size = sizeof(framebuffer) - 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_vision_image_validate(&image));
}

static void test_disabled_detectors_return_not_supported(void)
{
    uint8_t framebuffer[16 * 16] = { 0 };
    esp_vision_image_t image = {
        .width = 16,
        .height = 16,
        .pixformat = ESP_VISION_PIXFORMAT_GRAYSCALE,
        .data = framebuffer,
        .size = sizeof(framebuffer),
    };
    size_t count = 1;

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_vision_image_find_qrcodes(&image, NULL, NULL, 0, &count));
    TEST_ASSERT_EQUAL(0, count);
}

void app_main(void)
{
    esp_vision_core_init();

    UNITY_BEGIN();
    RUN_TEST(test_image_size_calculation);
    RUN_TEST(test_validate_and_draw_grayscale_line);
    RUN_TEST(test_disabled_detectors_return_not_supported);
    UNITY_END();

    esp_vision_core_deinit();
}
