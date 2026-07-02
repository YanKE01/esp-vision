# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

if((NOT DEFINED __idf_component_context) AND (NOT DEFINED COMPONENT_DIR))
    message(FATAL_ERROR
        "ESP-VISION root is an ESP-IDF component entry for esp-vision. "
        "For firmware builds use `make BOARD=<board> build`; for other IDF projects "
        "add this repository as a component dependency.")
endif()

set(ESP_VISION_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(ESP_VISION_CORE_DIR "${ESP_VISION_CORE_ROOT}/esp_vision_core")
set(ESP_VISION_IMLIB_DIR "${ESP_VISION_CORE_ROOT}/components/imlib")

set(ESP_VISION_CORE_SOURCES
    "${ESP_VISION_CORE_DIR}/esp_vision_core.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/collections.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/binary.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/bayer.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/edge.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/fmath.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/font.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/fsort.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/imlib.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/integral.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/mathop.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/point.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/rectangle.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/stats.c"
    "${ESP_VISION_IMLIB_DIR}/upstream/yuv.c"
    "${ESP_VISION_IMLIB_DIR}/compat/array.c"
    "${ESP_VISION_IMLIB_DIR}/compat/fb_alloc.c"
    "${ESP_VISION_IMLIB_DIR}/compat/framebuffer_compat.c"
    "${ESP_VISION_IMLIB_DIR}/compat/sincos_table.c"
    "${ESP_VISION_IMLIB_DIR}/compat/unaligned_memcpy.c"
    "${ESP_VISION_IMLIB_DIR}/compat/umm_malloc.c"
    "${ESP_VISION_CORE_DIR}/compat/codec_stubs.c"
)

if(CONFIG_ESP_VISION_CORE_ENABLE_LAB_LUT)
    list(APPEND ESP_VISION_CORE_SOURCES
        "${ESP_VISION_IMLIB_DIR}/compat/lab_table.c"
    )
endif()

if(CONFIG_ESP_VISION_CORE_ENABLE_DRAWING
        OR CONFIG_ESP_VISION_CORE_ENABLE_QRCODES
        OR CONFIG_ESP_VISION_CORE_ENABLE_APRILTAGS)
    list(APPEND ESP_VISION_CORE_SOURCES
        "${ESP_VISION_IMLIB_DIR}/upstream/draw.c"
    )
endif()

if(CONFIG_ESP_VISION_CORE_ENABLE_BLOBS)
    list(APPEND ESP_VISION_CORE_SOURCES
        "${ESP_VISION_IMLIB_DIR}/upstream/blob.c"
    )
endif()

if(CONFIG_ESP_VISION_CORE_ENABLE_QRCODES)
    list(APPEND ESP_VISION_CORE_SOURCES
        "${ESP_VISION_IMLIB_DIR}/upstream/qrcode.c"
    )
endif()

if(CONFIG_ESP_VISION_CORE_ENABLE_APRILTAGS)
    list(APPEND ESP_VISION_CORE_SOURCES
        "${ESP_VISION_IMLIB_DIR}/upstream/apriltag.c"
    )
endif()

if(CONFIG_ESP_VISION_CORE_ENABLE_FILTERS)
    list(APPEND ESP_VISION_CORE_SOURCES
        "${ESP_VISION_IMLIB_DIR}/upstream/filter.c"
    )
endif()

idf_component_register(
    SRCS ${ESP_VISION_CORE_SOURCES}
    INCLUDE_DIRS
        "${ESP_VISION_CORE_DIR}/include"
    PRIV_INCLUDE_DIRS
        "${ESP_VISION_CORE_DIR}/compat"
        "${ESP_VISION_IMLIB_DIR}/upstream"
        "${ESP_VISION_IMLIB_DIR}/include"
        "${ESP_VISION_IMLIB_DIR}/compat"
    REQUIRES heap esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CMSIS_MCU_H="cmsis_compiler.h"
    ESP_VISION_IMLIB_STANDALONE=1
    OMV_NO_GPL=1
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -Wno-error
)

if(DEFINED IDF_VERSION_MAJOR AND DEFINED IDF_VERSION_MINOR)
    set(ESP_VISION_IDF_VERSION "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")
elseif(DEFINED ENV{IDF_VERSION})
    set(ESP_VISION_IDF_VERSION "$ENV{IDF_VERSION}")
else()
    set(ESP_VISION_IDF_VERSION "0.0")
endif()

if(ESP_VISION_IDF_VERSION VERSION_GREATER_EQUAL "6.0")
    target_compile_options(${COMPONENT_LIB} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-std=gnu17>
        -Wno-uninitialized
        -Wno-maybe-uninitialized)
endif()
