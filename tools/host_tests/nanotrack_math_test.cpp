/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nanotrack_math.hpp"

#include <cassert>
#include <cstring>

int main()
{
    using namespace esp_vision::nanotrack;
    // P4 ties-to-even requantization and clipping at both signed limits.
    assert(quantize(1, 0.5f) == 0);
    assert(quantize(3, 0.5f) == 2);
    assert(quantize(-3, 0.5f) == -2);
    assert(quantize(255, 1) == 127);
    assert(quantize(-255, 1) == -128);
    assert(quantize(1, 0.5f, true) == 1);
    assert(quantize(-1, 0.5f, true) == 0);

    int8_t cls[256 * 2] = {};
    int8_t loc[256 * 4];
    float window[256] = {};
    std::memset(loc, 32, sizeof(loc));
    State state = {160, 120, 64, 64};
    State original = state;
    Box result = {};
    assert(!decode(cls, 1, loc, 1, window, 320, 240, 0.9f, state, result));
    assert(std::memcmp(&state, &original, sizeof(state)) == 0);

    // A high foreground logit at grid (9, 8) moves only x by one stride.
    int index = 8 * 16 + 9;
    cls[index * 2 + 1] = 100;
    assert(decode(cls, 1, loc, 1, window, 320, 240, 0.9f, state, result));
    assert(std::abs(state.cx - (160 + 16 * 128.0f / 127)) < 0.0001f);
    assert(state.cy == 120);
    assert(result.score > 0.99f);

    state = {319, 239, 64, 64};
    assert(decode(cls, 1, loc, 1, window, 320, 240, 0.9f, state, result));
    assert(result.x >= 0 && result.y >= 0);
    assert(result.x + result.w <= 320 && result.y + result.h <= 240);

    // Saturated/invalid box regressions must never move the search state.
    std::memset(loc, -1, sizeof(loc));
    original = state;
    assert(!decode(cls, 1, loc, 1, window, 320, 240, 0, state, result));
    assert(std::memcmp(&state, &original, sizeof(state)) == 0);
    return 0;
}
