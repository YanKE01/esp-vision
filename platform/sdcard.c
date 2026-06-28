/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "sdcard.h"

#include "esp_log.h"
#include "extmod/vfs.h"
#include "modmachine.h"
#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "soc/soc_caps.h"

#include "boardconfig.h"

#ifndef ESP_VISION_SDCARD_MOUNT_PATH
#define ESP_VISION_SDCARD_MOUNT_PATH "/sdcard"
#endif

#ifndef ESP_VISION_SDCARD_INTERFACE
#if SOC_SDMMC_HOST_SUPPORTED
#define ESP_VISION_SDCARD_INTERFACE ESP_VISION_SDCARD_INTERFACE_SDMMC
#else
#define ESP_VISION_SDCARD_INTERFACE ESP_VISION_SDCARD_INTERFACE_SDSPI
#endif
#endif

#if ESP_VISION_SDCARD_INTERFACE == ESP_VISION_SDCARD_INTERFACE_SDMMC
#if !SOC_SDMMC_HOST_SUPPORTED
#error "ESP_VISION_SDCARD_INTERFACE_SDMMC requires SOC_SDMMC_HOST_SUPPORTED"
#endif
#ifndef ESP_VISION_SDCARD_SLOT
#define ESP_VISION_SDCARD_SLOT (0)
#endif

#ifndef ESP_VISION_SDCARD_BUS_WIDTH
#define ESP_VISION_SDCARD_BUS_WIDTH (4)
#endif
#elif ESP_VISION_SDCARD_INTERFACE == ESP_VISION_SDCARD_INTERFACE_SDSPI
#ifndef ESP_VISION_SDCARD_SLOT
#define ESP_VISION_SDCARD_SLOT (2)
#endif
#else
#error "Unsupported ESP_VISION_SDCARD_INTERFACE"
#endif

#ifndef ESP_VISION_SDCARD_FREQ
#define ESP_VISION_SDCARD_FREQ (20000000)
#endif

#if !defined(ESP_VISION_SDCARD_SCK_PIN) && defined(ESP_VISION_SDCARD_CLK_PIN)
#define ESP_VISION_SDCARD_SCK_PIN ESP_VISION_SDCARD_CLK_PIN
#endif

#if ESP_VISION_SDCARD_INTERFACE == ESP_VISION_SDCARD_INTERFACE_SDMMC && \
    SOC_SDMMC_USE_GPIO_MATRIX && \
    defined(ESP_VISION_SDCARD_CLK_PIN) && \
    defined(ESP_VISION_SDCARD_CMD_PIN) && \
    defined(ESP_VISION_SDCARD_D0_PIN)
#define ESP_VISION_SDCARD_HAS_SDMMC_PIN_CONFIG (1)
#else
#define ESP_VISION_SDCARD_HAS_SDMMC_PIN_CONFIG (0)
#endif

#if ESP_VISION_SDCARD_INTERFACE == ESP_VISION_SDCARD_INTERFACE_SDSPI && \
    defined(ESP_VISION_SDCARD_MISO_PIN) && \
    defined(ESP_VISION_SDCARD_MOSI_PIN) && \
    defined(ESP_VISION_SDCARD_SCK_PIN) && \
    defined(ESP_VISION_SDCARD_CS_PIN)
#define ESP_VISION_SDCARD_HAS_SDSPI_PIN_CONFIG (1)
#else
#define ESP_VISION_SDCARD_HAS_SDSPI_PIN_CONFIG (0)
#endif

#if MICROPY_HW_ENABLE_SDCARD
static const char *TAG = "esp_vision_sdcard";
#endif

MP_WEAK void esp_vision_board_sdcard_init0(void)
{
}

MP_WEAK bool esp_vision_board_sdcard_is_present(void)
{
    return true;
}

MP_WEAK esp_err_t esp_vision_board_sdcard_preinit_host(sdmmc_host_t *host, int slot)
{
    (void)host;
    (void)slot;
    return ESP_OK;
}

MP_WEAK void esp_vision_board_sdcard_deinit_host(sdmmc_host_t *host, int slot)
{
    (void)host;
    (void)slot;
}

void esp_vision_sdcard_init0(void)
{
    esp_vision_board_sdcard_init0();
}

bool esp_vision_sdcard_is_present(void)
{
    return esp_vision_board_sdcard_is_present();
}

esp_err_t esp_vision_sdcard_preinit_host(sdmmc_host_t *host, int slot)
{
    if ((host == NULL) || (slot != ESP_VISION_SDCARD_SLOT)) {
        return ESP_OK;
    }

    if (!esp_vision_sdcard_is_present()) {
        return ESP_ERR_NOT_FOUND;
    }

    return esp_vision_board_sdcard_preinit_host(host, slot);
}

void esp_vision_sdcard_deinit_host(sdmmc_host_t *host, int slot)
{
    if ((host == NULL) || (slot != ESP_VISION_SDCARD_SLOT)) {
        return;
    }

    esp_vision_board_sdcard_deinit_host(host, slot);
}

void esp_vision_sdcard_mount_if_present(void)
{
#if MICROPY_HW_ENABLE_SDCARD
    if (!esp_vision_sdcard_is_present()) {
        return;
    }

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        const char *path = ESP_VISION_SDCARD_MOUNT_PATH;
        mp_obj_t sdcard_kwargs[16];
        size_t sdcard_kwarg_count = 0;
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_slot);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_SLOT);
        sdcard_kwarg_count++;

        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_freq);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_FREQ);
        sdcard_kwarg_count++;

#if ESP_VISION_SDCARD_INTERFACE == ESP_VISION_SDCARD_INTERFACE_SDMMC
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_width);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_BUS_WIDTH);
        sdcard_kwarg_count++;

#if ESP_VISION_SDCARD_HAS_SDMMC_PIN_CONFIG
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_sck);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_CLK_PIN);
        sdcard_kwarg_count++;
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_cmd);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_CMD_PIN);
        sdcard_kwarg_count++;

#if ESP_VISION_SDCARD_BUS_WIDTH == 1
        mp_obj_t data_pins[] = {
            MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_D0_PIN),
        };
#elif ESP_VISION_SDCARD_BUS_WIDTH == 4 && \
    defined(ESP_VISION_SDCARD_D1_PIN) && \
    defined(ESP_VISION_SDCARD_D2_PIN) && \
    defined(ESP_VISION_SDCARD_D3_PIN)
        mp_obj_t data_pins[] = {
            MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_D0_PIN),
            MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_D1_PIN),
            MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_D2_PIN),
            MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_D3_PIN),
        };
#else
#error "ESP_VISION_SDCARD_BUS_WIDTH requires matching ESP_VISION_SDCARD_Dx_PIN definitions"
#endif
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_data);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = mp_obj_new_tuple(MP_ARRAY_SIZE(data_pins), data_pins);
        sdcard_kwarg_count++;
#endif
#elif ESP_VISION_SDCARD_INTERFACE == ESP_VISION_SDCARD_INTERFACE_SDSPI
#if ESP_VISION_SDCARD_HAS_SDSPI_PIN_CONFIG
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_miso);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_MISO_PIN);
        sdcard_kwarg_count++;
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_mosi);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_MOSI_PIN);
        sdcard_kwarg_count++;
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_sck);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_SCK_PIN);
        sdcard_kwarg_count++;
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_cs);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_CS_PIN);
        sdcard_kwarg_count++;
#endif
#endif

#if defined(ESP_VISION_SDCARD_CD_PIN)
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_cd);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_CD_PIN);
        sdcard_kwarg_count++;
#endif

#if defined(ESP_VISION_SDCARD_WP_PIN)
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 0] = MP_OBJ_NEW_QSTR(MP_QSTR_wp);
        sdcard_kwargs[(2 * sdcard_kwarg_count) + 1] = MP_OBJ_NEW_SMALL_INT(ESP_VISION_SDCARD_WP_PIN);
        sdcard_kwarg_count++;
#endif

        mp_obj_t sdcard = mp_call_function_n_kw(MP_OBJ_FROM_PTR(&machine_sdcard_type),
                                                0,
                                                sdcard_kwarg_count,
                                                sdcard_kwargs);
        mp_obj_t mount_args[] = {
            sdcard,
            mp_obj_new_str(path, strlen(path)),
        };
        mp_vfs_mount(2, mount_args, (mp_map_t *)&mp_const_empty_map);
        nlr_pop();
        ESP_LOGI(TAG, "mounted SD card at %s", path);
    } else {
        mp_obj_t exc = MP_OBJ_FROM_PTR(nlr.ret_val);
        mp_printf(&mp_plat_print, "[esp-vision] SD mount failed: ");
        mp_obj_print_exception(&mp_plat_print, exc);
    }
#endif
}
