# Touch-selected NanoTrack V2

Use a firmware build containing `espdl.NanoTrack`, `display` and `lcd_touch`. The touch example targets ESP32-P4 Function EV Board and ESP32-S31 Mosaico; P4X-EYE supports the tracker but has no configured touch controller. Build with the normal repository-root `idf.py --board <BOARD> build` entry point.

## Prepare the full-INT8 models

Copy the three `.espdl` files in [`models/nanotrack_v2`](../../../models/nanotrack_v2/README.md) to `/sdcard/nanotrack_v2/`. The example's default `MODEL_DIR` already matches this layout; no local quantization is needed to use these prepared files.

To regenerate the models instead, run from the repository root:

```sh
cd experiments/nanotrack-v2
uv run --locked fetch_assets.py
uv run --locked experiment.py prepare
uv run --locked experiment.py quantize --calibration-count 32
uv run --locked experiment.py export
```

The regenerated files are `quantized/template_deploy.espdl`, `quantized/search_deploy.espdl` and `quantized/head_deploy.espdl`. These three full-INT8 files total 897920 bytes for the initial experiment. Do not use the mixed INT16 files from `study/backbone16/`. Test videos and other experiment outputs remain local. If using `/flash`, change `MODEL_DIR` in the example accordingly.

## Select and track

Run `nanotrack_touch.py`. Drag one finger diagonally across the object, then release to start tracking. The view freezes during selection, keeping the selected pixels consistent with the template. Touch and drag again to select a new target. A short tap or rectangle smaller than 10x10 camera pixels is rejected. Initial touches in the display's black margins are ignored.

The example uses 320x240 RGB565 images. Display scaling and touch mapping share the same transform, so a 1024x600 or square LCD does not require changing ROI coordinates. Selection and tracking annotations are drawn on a separate reusable canvas. Template extraction and tracking always receive unannotated camera pixels.

The template model loads during `init()` and is released after its features are cached. Search and head models remain loaded, and run once per tracked frame. Reselection temporarily needs memory for all three models. This first native implementation uses the INT8 model tensor layouts and explicitly requantizes features between model exponents.

After ten consecutive below-threshold results the example asks for a new selection. This is not global target reacquisition; an incorrect match can still have a high score. The PC experiment is an accuracy reference, not evidence of device FPS or bit-exact hardware agreement. Native RGB565 conversion, bilinear rounding and ESP-DL activation LUTs can differ from PC execution. Confirm tracking accuracy and resource usage on the target board before relying on it.
