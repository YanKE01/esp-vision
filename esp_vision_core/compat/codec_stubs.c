/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "imlib.h"

void jpeg_decompress(image_t *dst, image_t *src)
{
    (void)dst;
    (void)src;
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("JPEG decode is not enabled"));
}

void png_decompress(image_t *dst, image_t *src)
{
    (void)dst;
    (void)src;
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("PNG decode is not enabled"));
}
