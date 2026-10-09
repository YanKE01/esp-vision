# NanoTrack V2 INT8

Three full-INT8 ESP-DL models for `espdl.NanoTrack`. The exports target ESP32-P4 / PIE V2 and are intended for P4/S31; S3 requires target-specific conversion and validation. These are the original all-INT8 experiment outputs, without embedded test inputs or outputs. They are not the mixed INT16 accuracy experiment.

| File | Role | Size |
| --- | --- | ---: |
| `template_deploy.espdl` | 127x127 template backbone, used during selection | 328096 bytes |
| `search_deploy.espdl` | 255x255 search backbone, used every frame | 328096 bytes |
| `head_deploy.espdl` | Template/search feature matching and box prediction | 241728 bytes |
| **Total** | All three files required | **897920 bytes (about 877 KiB)** |

## Usage

Copy this folder to `/sdcard/nanotrack_v2/`, then run [`nanotrack_touch.py`](../../example/03-Machine-Learning/00-ESP-DL/nanotrack_touch.py) on a touch-enabled board. The example's default paths already match this layout. On a board using `/flash`, change `MODEL_DIR` in the example.

```python
import espdl

tracker = espdl.NanoTrack(
    "/sdcard/nanotrack_v2/template_deploy.espdl",
    "/sdcard/nanotrack_v2/search_deploy.espdl",
    "/sdcard/nanotrack_v2/head_deploy.espdl",
)
```

The template model loads during `init()` and is freed after caching its features. Search and head models stay loaded during tracking. The files are not compiled into firmware, and their sizes do not represent peak runtime RAM usage. Device tracking accuracy and frame rate still require hardware validation.

## Provenance and reproduction

Source: [HonglinChu/SiamTrackers, commit 248663f](https://github.com/HonglinChu/SiamTrackers/tree/248663fde6bf7c40190cf10ee396d5662919ecd3/NanoTrack/models/nanotrackv2). Conversion uses ESP-PPQ 1.3.11 with ESP32-P4 W8A8 quantization and KL calibration on 32 OTB David template/search pairs. See [`experiments/nanotrack-v2`](../../experiments/nanotrack-v2/README.md) for the locked uv environment, conversion commands and PC accuracy results.

[`provenance.json`](provenance.json) records the original ONNX URLs and hashes, conversion settings and hashes of these exact exports. Each model also has a metadata sidecar with its byte count and SHA-256.

The upstream NanoTrack directory provides the Apache-2.0 text copied verbatim in [`UPSTREAM_LICENSE`](UPSTREAM_LICENSE). A separate declaration for these pretrained weight files has not been verified; weight and training-data redistribution terms must not be inferred from the code license. The repository's Apache-2.0 license does not override upstream asset terms.
