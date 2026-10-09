# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Local NanoTrack V2 ESP32-P4 quantization experiment; no firmware changes."""
from __future__ import annotations

import argparse
import collections
import json
import os
from pathlib import Path
import shutil
import time

os.environ.setdefault("OMP_NUM_THREADS", "4")
os.environ.setdefault("MPLCONFIGDIR", str(Path(__file__).parent / "mpl-cache"))

import cv2
import numpy as np
import onnx
import onnxruntime as ort
import torch

ROOT = Path(__file__).resolve().parent
torch.set_num_threads(4)
cv2.setNumThreads(1)


def write_json(path, obj):
    Path(path).write_text(json.dumps(obj, indent=2))


def session(path):
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 4
    opts.inter_op_num_threads = 1
    return ort.InferenceSession(str(path), opts, providers=["CPUExecutionProvider"])


def tensor_names(branch):
    # Exporters may reorder the graph's input/output lists. Bind by name.
    return (["input1", "input2"], ["output1", "output2"]) if branch == "head" else (["input"], ["output"])


def sequence(name):
    """OpenCV David video starts 299 frames before its first annotation."""
    folder = ROOT / "data" / name
    gt = np.loadtxt(folder / "gt.txt", delimiter=",").astype(np.float32)
    # OTB annotations use one-based image coordinates.
    gt[:, :2] -= 1
    start = 299 if name == "david" else 0
    cap = cv2.VideoCapture(str(folder / "video.webm"))
    frames = []
    index = 0
    while True:
        ok, frame = cap.read()
        if not ok:
            break
        if index >= start:
            frames.append(frame)
        index += 1
    cap.release()
    if len(frames) != len(gt):
        raise ValueError(f"{name}: {len(frames)} frames != {len(gt)} annotations")
    return frames, gt


def crop(frame, center, side, output_size, padding):
    side = max(2, int(round(float(side))))
    x, y = np.floor(np.asarray(center) - (side + 1) / 2 + 0.5).astype(int)
    h, w = frame.shape[:2]
    left, top = max(0, -x), max(0, -y)
    right, bottom = max(0, x + side - w), max(0, y + side - h)
    if left or top or right or bottom:
        frame = cv2.copyMakeBorder(frame, top, bottom, left, right, cv2.BORDER_CONSTANT,
                                   value=tuple(float(v) for v in padding))
    patch = frame[y + top:y + top + side, x + left:x + left + side]
    patch = cv2.resize(patch, (output_size, output_size), interpolation=cv2.INTER_LINEAR)
    # PyTorch upstream tracker uses unnormalized BGR, not ImageNet normalization.
    return np.ascontiguousarray(patch.transpose(2, 0, 1)[None], dtype=np.float32)


def extent(size):
    pad = 0.5 * np.sum(size)
    return float(np.sqrt((size[0] + pad) * (size[1] + pad)))


def prepare():
    prepared = ROOT / "prepared"
    prepared.mkdir(exist_ok=True)
    inventory = {}
    for name, size in [("template", 127), ("search", 255)]:
        model = onnx.load(ROOT / "models/nanotrack_backbone_sim.onnx")
        for dim, val in zip(model.graph.input[0].type.tensor_type.shape.dim, [1, 3, size, size]):
            dim.dim_value = val
        model.graph.ClearField("value_info")
        model.graph.output[0].type.tensor_type.ClearField("shape")
        model = onnx.shape_inference.infer_shapes(model)
        onnx.checker.check_model(model)
        onnx.save(model, prepared / f"{name}.onnx")
    shutil.copyfile(ROOT / "models/nanotrack_head_sim.onnx", prepared / "head.onnx")
    for path in prepared.glob("*.onnx"):
        sess = session(path)
        outs = sess.run(None, {x.name: np.zeros(x.shape, np.float32) for x in sess.get_inputs()})
        inventory[path.stem] = {
            "inputs": {x.name: x.shape for x in sess.get_inputs()},
            "outputs": {x.name: list(v.shape) for x, v in zip(sess.get_outputs(), outs)},
            "operators": dict(collections.Counter(n.op_type for n in onnx.load(path).graph.node)),
        }
    write_json(ROOT / "model_inventory.json", inventory)
    print(json.dumps(inventory, indent=2), flush=True)


def calibration(count, manifest_path=None, strategy="local"):
    frames, gt = sequence("david")
    rng = np.random.default_rng(20261008)
    pairs = []
    samples = []
    for sample_idx, i in enumerate(np.linspace(0, len(frames) - 1, count, dtype=int)):
        j = max(0, i - int(rng.integers(1, 40)))
        if strategy == "cached" and sample_idx % 2 == 0:
            j = 0
        zbox, xbox = gt[j], gt[i]
        zcenter = zbox[:2] + (zbox[2:] - 1) / 2
        xcenter = xbox[:2] + (xbox[2:] - 1) / 2
        # Include mild localization error representative of a live tracker.
        xcenter += rng.normal(0, 0.08, 2) * xbox[2:]
        z = crop(frames[j], zcenter, extent(zbox[2:]), 127, frames[j].mean((0, 1)))
        x = crop(frames[i], xcenter, extent(xbox[2:]) * 255 / 127, 255, frames[j].mean((0, 1)))
        pairs.append((z, x))
        samples.append({"template_annotated_index": int(j), "search_annotated_index": int(i),
                        "template_video_index": int(j + 299), "search_video_index": int(i + 299)})
    write_json(manifest_path or ROOT / "calibration_manifest.json", {"sequence": "david", "count": count, "strategy": strategy,
               "seed": 20261008, "held_out_sequence": "faceocc2", "samples": samples})
    return pairs


class Backend:
    def __init__(self, quantized=False, qdq=False):
        self.quantized = quantized
        if quantized:
            from esp_ppq.api import load_native_graph
            from esp_ppq.executor import TorchExecutor
            self.models = {name: TorchExecutor(load_native_graph(str(ROOT / "quantized" / f"{name}.native")), device="cpu")
                           for name in ("template", "search", "head")}
        else:
            self.models = {name: session(ROOT / "quantized" / f"{name}_int8.onnx" if qdq else ROOT / "prepared" / f"{name}.onnx")
                           for name in ("template", "search", "head")}

    def run(self, name, arrays):
        model = self.models[name]
        input_names, output_names = tensor_names(name)
        if self.quantized:
            inputs = {key: torch.from_numpy(np.ascontiguousarray(a)) for key, a in zip(input_names, arrays)}
            with torch.no_grad():
                out = model.forward(inputs=inputs, output_names=output_names)
            return [x.detach().cpu().numpy() for x in out]
        return model.run(output_names, dict(zip(input_names, arrays)))


def quantize(count):
    from esp_ppq.api import espdl_quantize_onnx, export_ppq_graph
    from esp_ppq.api.espdl_interface import generate_test_value
    from esp_ppq.core import TargetPlatform
    from esp_ppq.executor import TorchExecutor

    outdir = ROOT / "quantized"
    outdir.mkdir(exist_ok=True)
    pairs = calibration(count)
    executors = {}
    results = {}
    for name in ("template", "search", "head"):
        if name in ("template", "search"):
            inputs = [[torch.from_numpy(pair[0 if name == "template" else 1])] for pair in pairs]
        else:
            # Calibrate head on quantized backbone outputs, as used at inference.
            with torch.no_grad():
                inputs = [[executors["template"].forward(inputs=[torch.from_numpy(z)])[0].clone(),
                           executors["search"].forward(inputs=[torch.from_numpy(x)])[0].clone()]
                          for z, x in pairs]
        print(f"Quantizing {name}: {len(inputs)} real samples", flush=True)
        t0 = time.perf_counter()
        graph = espdl_quantize_onnx(
            onnx_import_file=str(ROOT / "prepared" / f"{name}.onnx"),
            espdl_export_file=str(outdir / f"{name}.espdl"),
            calib_dataloader=inputs, calib_steps=len(inputs),
            input_shape=[list(x.shape) for x in inputs[0]], inputs=inputs[0],
            target="esp32p4", num_of_bits=8, quant_type="w8a8", device="cpu",
            error_report=False, skip_export=True, collate_fn=lambda batch: batch,
        )
        export_ppq_graph(graph, TargetPlatform.NATIVE, str(outdir / f"{name}.native"))
        test_values = generate_test_value(graph, "cpu", inputs[0], None)
        export_ppq_graph(graph, TargetPlatform.ESPDL_INT8, str(outdir / f"{name}.espdl"),
                         values_for_test=test_values, export_config=True)
        executors[name] = TorchExecutor(graph, device="cpu")
        with torch.no_grad():
            values = executors[name].forward(inputs=inputs[0])
        if not all(torch.isfinite(v).all() for v in values):
            raise RuntimeError(f"Non-finite output: {name}")
        results[name] = {"seconds": time.perf_counter() - t0,
                         "espdl_bytes": (outdir / f"{name}.espdl").stat().st_size,
                         "platforms": dict(collections.Counter(op.platform.name for op in graph.operations.values())),
                         "output_shapes": [list(v.shape) for v in values]}
        write_json(ROOT / "quantization_summary.json", results)
        print(f"Exported {name}: {results[name]}", flush=True)


def export_pc():
    """Export runnable ONNX QDQ and .espdl without embedded test tensors."""
    from esp_ppq.api import export_ppq_graph, load_native_graph
    from esp_ppq.core import TargetPlatform

    outdir = ROOT / "quantized"
    frames, gt = sequence("faceocc2")
    center = gt[0, :2] + (gt[0, 2:] - 1) / 2
    padding = frames[0].mean((0, 1))
    z = crop(frames[0], center, extent(gt[0, 2:]), 127, padding)
    x = crop(frames[1], center, extent(gt[0, 2:]) * 255 / 127, 255, padding)
    backend = Backend(True)
    feeds = {"template": [z], "search": [x]}
    feeds["head"] = [backend.run("template", [z])[0], backend.run("search", [x])[0]]
    results = {}
    for name in ["template", "search", "head"]:
        graph = load_native_graph(str(outdir / f"{name}.native"))
        export_ppq_graph(graph, TargetPlatform.ESPDL_INT8, str(outdir / f"{name}_deploy.espdl"), export_config=False)
        path = outdir / f"{name}_int8.onnx"
        export_ppq_graph(graph, TargetPlatform.ONNXRUNTIME, str(path), remove_activation=False)
        qdq = onnx.load(path)
        # PPQ's ORT exporter hardcodes opset 13, but this backbone contains
        # HardSwish (introduced in 14). All other exported nodes retain the
        # same schema semantics at 14, including Q/DQ, Conv, Gemm and Reshape.
        for opset in qdq.opset_import:
            if opset.domain == "" and opset.version < 14:
                opset.version = 14
        onnx.checker.check_model(qdq)
        onnx.save(qdq, path)
        runtime = session(path)
        input_names, output_names = tensor_names(name)
        actual = runtime.run(output_names, dict(zip(input_names, feeds[name])))
        expected = backend.run(name, feeds[name])
        results[name] = {
            "deploy_espdl_bytes": (outdir / f"{name}_deploy.espdl").stat().st_size,
            "qdq_onnx_bytes": path.stat().st_size,
            "finite_outputs": all(bool(np.isfinite(a).all()) for a in actual),
            "qdq_vs_ppq_max_abs_error": [float(np.abs(a - e).max()) for a, e in zip(actual, expected)],
        }
        if not results[name]["finite_outputs"]:
            raise RuntimeError(f"Non-finite QDQ output: {name}")
        print(name, results[name], flush=True)
    write_json(ROOT / "export_summary.json", results)


class Tracker:
    def __init__(self, backend, frame, box):
        self.backend = backend
        self.center = np.asarray(box[:2], np.float32) + (np.asarray(box[2:]) - 1) / 2
        self.size = np.asarray(box[2:], np.float32).copy()
        self.padding = frame.mean((0, 1))
        z = crop(frame, self.center, extent(self.size), 127, self.padding)
        self.template = backend.run("template", [z])[0]
        self.window = np.outer(np.hanning(16), np.hanning(16)).ravel()
        grid = (np.arange(16) - 8) * 16
        x, y = np.meshgrid(grid, grid)
        self.points = np.stack([x.ravel(), y.ravel()])

    def track(self, frame):
        scale = 127 / extent(self.size)
        x = crop(frame, self.center, extent(self.size) * 255 / 127, 255, self.padding)
        xf = self.backend.run("search", [x])[0]
        cls, loc = self.backend.run("head", [self.template, xf])
        logits = cls.reshape(2, -1)
        prob = np.exp(logits - logits.max(axis=0))
        score = prob[1] / prob.sum(axis=0)
        loc = loc.reshape(4, -1)
        widths, heights = loc[0] + loc[2], loc[1] + loc[3]
        widths, heights = np.maximum(widths, 1e-6), np.maximum(heights, 1e-6)
        predicted_center = self.points + np.stack([(loc[2] - loc[0]) / 2, (loc[3] - loc[1]) / 2])
        pad = (widths + heights) * 0.5
        size_change = np.sqrt((widths + pad) * (heights + pad)) / (extent(self.size) * scale)
        ratio_change = (self.size[0] / self.size[1]) / (widths / heights)
        size_change = np.maximum(size_change, 1 / size_change)
        ratio_change = np.maximum(ratio_change, 1 / ratio_change)
        penalty = np.exp(-(size_change * ratio_change - 1) * 0.150)
        weighted = penalty * score * (1 - 0.490) + self.window * 0.490
        best = int(np.argmax(weighted))
        rate = penalty[best] * score[best] * 0.385
        self.center += predicted_center[:, best] / scale
        self.size = self.size * (1 - rate) + np.array([widths[best], heights[best]]) / scale * rate
        self.center = np.clip(self.center, [0, 0], [frame.shape[1], frame.shape[0]])
        self.size = np.clip(self.size, [10, 10], [frame.shape[1], frame.shape[0]])
        return np.r_[self.center - self.size / 2, self.size], float(score[best])


def iou(a, b):
    a, b = np.asarray(a), np.asarray(b)
    lo = np.maximum(a[:2], b[:2]); hi = np.minimum(a[:2] + a[2:], b[:2] + b[2:])
    area = np.maximum(hi - lo, 0).prod()
    return float(area / max(1e-6, a[2:].prod() + b[2:].prod() - area))


def evaluate(limit, int8_backend="onnx"):
    """CPU fake-quant simulation of ESP-DL rules, not .espdl hardware execution."""
    frames, gt = sequence("faceocc2")
    limit = min(limit, len(frames))
    backends = [Backend(False), Backend(quantized=int8_backend == "ppq", qdq=int8_backend == "onnx")]
    trackers = [Tracker(b, frames[0], gt[0]) for b in backends]
    rows = []
    out = ROOT / ("evaluation_qdq" if int8_backend == "onnx" else "evaluation")
    out.mkdir(exist_ok=True)
    height, width = frames[0].shape[:2]
    video = cv2.VideoWriter(str(out / "comparison.mp4"), cv2.VideoWriter_fourcc(*"avc1"), 25, (width * 2, height))
    if not video.isOpened():
        raise RuntimeError("H264 video writer unavailable")
    for i in range(1, limit):
        panels = []
        row = {"frame": i, "gt": gt[i].tolist()}
        for label, tracker in zip(["fp32", "int8"], trackers):
            t0 = time.perf_counter()
            box, score = tracker.track(frames[i])
            row[label] = {"box": box.tolist(), "score": score, "iou": iou(box, gt[i]),
                          "cpu_simulation_ms": (time.perf_counter() - t0) * 1000}
            panel = frames[i].copy()
            for rect, color in [(gt[i], (0, 220, 0)), (box, (0, 170, 255))]:
                x, y, w, h = np.round(rect).astype(int)
                cv2.rectangle(panel, (x, y), (x + w, y + h), color, 2)
            cv2.rectangle(panel, (0, 0), (width, 28), (20, 20, 20), -1)
            cv2.rectangle(panel, (0, height - 30), (width, height), (20, 20, 20), -1)
            cv2.putText(panel, f"{label.upper()}  frame {i}", (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 1, cv2.LINE_AA)
            cv2.putText(panel, f"IoU {row[label]['iou']:.2f}  score {score:.2f}", (8, height - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
            panels.append(panel)
        combined = np.concatenate(panels, axis=1)
        video.write(combined)
        if i in [1, 50, 100, 200, 400, 600, limit - 1]:
            cv2.imwrite(str(out / f"frame_{i:04d}.jpg"), combined)
        row["fp32_int8_iou"] = iou(row["fp32"]["box"], row["int8"]["box"])
        rows.append(row)
        if i % 25 == 0:
            print(f"Evaluated {i}/{limit - 1}: FP32 IoU={row['fp32']['iou']:.3f}, INT8 IoU={row['int8']['iou']:.3f}", flush=True)
    video.release()
    write_json(out / "frames.json", rows)
    summary = {"sequence": "faceocc2", "frames_evaluated": len(rows), "calibration_sequence": "david",
               "backend": "FP32 ONNX Runtime vs " + ("ONNX Runtime Q/DQ" if int8_backend == "onnx" else "ESP-PPQ CPU fake-quant") + " (ESP32-P4 w8a8)",
               "warning": "Not hardware inference; neither accuracy benchmark coverage nor MCU FPS measurement."}
    for label in ["fp32", "int8"]:
        scores = np.array([r[label]["iou"] for r in rows])
        summary[label] = {"mean_iou": float(scores.mean()), "fraction_iou_ge_0_5": float(np.mean(scores >= 0.5)),
                          "mean_cpu_simulation_ms": float(np.mean([r[label]["cpu_simulation_ms"] for r in rows]))}
    summary["mean_fp32_int8_iou"] = float(np.mean([r["fp32_int8_iou"] for r in rows]))
    write_json(out / "summary.json", summary)
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=["prepare", "quantize", "export", "evaluate"])
    parser.add_argument("--calibration-count", type=int, default=32)
    parser.add_argument("--frames", type=int, default=200)
    parser.add_argument("--int8-backend", choices=["onnx", "ppq"], default="onnx")
    args = parser.parse_args()
    if args.stage == "prepare":
        prepare()
    elif args.stage == "quantize":
        quantize(args.calibration_count)
    elif args.stage == "export":
        export_pc()
    else:
        evaluate(args.frames, args.int8_backend)
