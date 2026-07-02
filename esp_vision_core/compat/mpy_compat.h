/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal MicroPython compatibility helpers used only while building
 * esp_vision_core's private imlib sources as an ESP-IDF component.
 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"

#ifndef NORETURN
#define NORETURN __attribute__((noreturn))
#endif

#ifndef MP_ERROR_TEXT
#define MP_ERROR_TEXT(s) (s)
#endif

typedef const char *mp_rom_error_text_t;
typedef void *mp_obj_t;

typedef struct {
    const char *name;
} esp_vision_core_exception_type_t;

static const esp_vision_core_exception_type_t mp_type_MemoryError = { "MemoryError" };
static const esp_vision_core_exception_type_t mp_type_OSError = { "OSError" };
static const esp_vision_core_exception_type_t mp_type_RuntimeError = { "RuntimeError" };

NORETURN static inline void esp_vision_core_abort(const char *type, const char *msg)
{
    printf("esp_vision_core imlib %s: %s\n", type ? type : "error", msg ? msg : "unknown error");
    abort();
}

NORETURN static inline void esp_vision_core_abort_varg(const char *type, const char *fmt, ...)
{
    printf("esp_vision_core imlib %s: ", type ? type : "error");
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt ? fmt : "unknown error", ap);
    va_end(ap);
    printf("\n");
    abort();
}

#define mp_raise_msg(type, msg) \
    esp_vision_core_abort(((const esp_vision_core_exception_type_t *)(type))->name, (msg))
#define mp_raise_msg_varg(type, fmt, ...) \
    esp_vision_core_abort_varg(((const esp_vision_core_exception_type_t *)(type))->name, (fmt), ##__VA_ARGS__)
#define mp_raise_OSError(err) esp_vision_core_abort_varg("OSError", "errno %d", (int)(err))

static inline void *m_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Out of memory"));
    }
    return ptr;
}

static inline void *m_malloc0(size_t size)
{
    void *ptr = m_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

static inline void *m_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if ((new_ptr == NULL) && (size != 0)) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Out of memory"));
    }
    return new_ptr;
}

static inline void m_free(void *ptr)
{
    free(ptr);
}

#define m_new_obj(type) ((type *)m_malloc(sizeof(type)))
#define MP_STACK_CHECK()

static inline uint32_t mp_hal_ticks_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static inline uint32_t mp_hal_ticks_us(void)
{
    return (uint32_t)esp_timer_get_time();
}
