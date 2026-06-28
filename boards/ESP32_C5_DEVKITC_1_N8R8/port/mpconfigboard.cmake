set(IDF_TARGET esp32c5)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.riscv
    boards/sdkconfig.240mhz
    boards/sdkconfig.free_ram
    boards/ESP32_C5_DEVKITC_1_N8R8/sdkconfig.c5_devkitc_1_n8r8
    boards/ESP32_C5_DEVKITC_1_N8R8/sdkconfig.board
)

# Keep early bring-up independent of optional MicroPython submodules.
set(MICROPY_PY_BTREE OFF)
list(APPEND MICROPY_CPP_FLAGS_EXTRA
    -DMICROPY_PY_BLUETOOTH=0
)
