# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Select quantization settings on Dudek, then evaluate once on Faceocc2."""
import argparse
import json
from pathlib import Path
import time

import experiment as e
import cv2
import numpy as np
import onnx
import torch
from esp_ppq.api import espdl_quantize_onnx, export_ppq_graph, load_native_graph
from esp_ppq.api.setting import QuantizationSettingFactory
from esp_ppq.core import TargetPlatform
from esp_ppq.executor import TorchExecutor

STUDY = e.ROOT / "study"


class Branches:
    def __init__(self, paths):
        self.models = {}
        for name, path in paths.items():
            path = Path(path)
            self.models[name] = ("ppq", TorchExecutor(load_native_graph(str(path)), device="cpu")) if path.suffix == ".native" else ("onnx", e.session(path))

    def run(self, name, arrays):
        kind, model = self.models[name]
        input_names, output_names = e.tensor_names(name)
        if kind == "onnx":
            return model.run(output_names, dict(zip(input_names, arrays)))
        with torch.no_grad():
            inputs = {key: torch.from_numpy(np.ascontiguousarray(a)) for key, a in zip(input_names, arrays)}
            return [x.numpy() for x in model.forward(inputs=inputs, output_names=output_names)]


def paths(folder, suffix):
    return {name: str(folder / f"{name}{suffix}") for name in ("template", "search", "head")}


def score_case(name, model_paths, sequence_name):
    """Streaming evaluation; ground truth never feeds the tracker after init."""
    folder = e.ROOT / "data" / sequence_name
    gt = np.loadtxt(folder / "gt.txt", delimiter=",").astype(np.float32)
    gt[:, :2] -= 1
    cap = cv2.VideoCapture(str(folder / "video.webm"))
    ok, first = cap.read()
    if not ok:
        raise ValueError("Cannot open video")
    tracker = e.Tracker(Branches(model_paths), first, gt[0])
    rows = []
    t0 = time.perf_counter()
    for index in range(1, len(gt)):
        ok, frame = cap.read()
        if not ok:
            raise ValueError("Video ended before annotations")
        box, confidence = tracker.track(frame)
        rows.append({"frame": index, "box": box.tolist(), "score": confidence, "iou": e.iou(box, gt[index])})
        if index % 300 == 0:
            print(f"{sequence_name}/{name}: {index}/{len(gt)-1}", flush=True)
    ok, _ = cap.read()
    cap.release()
    if ok:
        raise ValueError("More video frames than annotations")
    outdir = STUDY / sequence_name
    outdir.mkdir(exist_ok=True)
    e.write_json(outdir / f"{name}_frames.json", rows)
    overlaps = np.array([r["iou"] for r in rows])
    result = {"mean_iou": float(overlaps.mean()), "fraction_iou_ge_0_5": float(np.mean(overlaps >= 0.5)),
              "frames": len(rows), "seconds": time.perf_counter() - t0, "models": model_paths}
    print(f"{sequence_name}/{name}: mean IoU={result['mean_iou']:.6f}", flush=True)
    return result


def quantize_case(name, pairs, algorithm="kl", head_bits=8, backbone_bits=8, strategy="cached"):
    folder = STUDY / name
    folder.mkdir(exist_ok=True)
    executors = {}
    outputs = {}
    for branch in ["template", "search", "head"]:
        bits = head_bits if branch == "head" else backbone_bits
        if branch == "head":
            with torch.no_grad():
                data = [[executors["template"].forward(inputs=[torch.from_numpy(z)])[0].clone(),
                         executors["search"].forward(inputs=[torch.from_numpy(x)])[0].clone()] for z, x in pairs]
        else:
            data = [[torch.from_numpy(pair[branch == "search"])] for pair in pairs]
        setting = QuantizationSettingFactory.espdl_setting(num_of_bits=bits)
        setting.quantize_activation_setting.calib_algorithm = "minmax" if bits == 16 else algorithm
        graph = espdl_quantize_onnx(
            onnx_import_file=str(e.ROOT / "prepared" / f"{branch}.onnx"),
            espdl_export_file=str(folder / f"{branch}.espdl"),
            calib_dataloader=data, calib_steps=len(data), inputs=data[0],
            input_shape=[list(v.shape) for v in data[0]], target="esp32p4",
            num_of_bits=bits, setting=setting, device="cpu", error_report=False,
            skip_export=True, collate_fn=lambda batch: batch,
        )
        export_ppq_graph(graph, TargetPlatform.NATIVE, str(folder / f"{branch}.native"))
        export_ppq_graph(graph, TargetPlatform.ESPDL_INT16 if bits == 16 else TargetPlatform.ESPDL_INT8,
                         str(folder / f"{branch}.espdl"), export_config=True)
        executors[branch] = TorchExecutor(graph, device="cpu")
        with torch.no_grad():
            smoke = executors[branch].forward(inputs=data[0])
        if not all(bool(torch.isfinite(v).all()) for v in smoke):
            raise RuntimeError(f"Non-finite quantized output: {name}/{branch}")
        if bits == 8:
            path = folder / f"{branch}.onnx"
            export_ppq_graph(graph, TargetPlatform.ONNXRUNTIME, str(path), remove_activation=False)
            model = onnx.load(path)
            for opset in model.opset_import:
                if opset.domain == "" and opset.version < 14:
                    opset.version = 14
            onnx.checker.check_model(model)
            onnx.save(model, path)
            runtime = e.session(path)
            input_names, output_names = e.tensor_names(branch)
            # Verify several calibration samples; keep the PPQ reference if
            # ONNX export ever disagrees with the original quantization graph.
            for sample in data[:3]:
                actual = runtime.run(output_names, {key: v.numpy() for key, v in zip(input_names, sample)})
                with torch.no_grad():
                    expected = executors[branch].forward(inputs=dict(zip(input_names, sample)), output_names=output_names)
                for a, b in zip(actual, expected):
                    np.testing.assert_allclose(a, b.numpy(), rtol=1e-5, atol=1e-5)
        else:
            path = folder / f"{branch}.native"
        outputs[branch] = str(path)
    e.write_json(folder / "settings.json", {"algorithm": algorithm, "samples": len(pairs),
                 "template_strategy": strategy, "backbone_bits": backbone_bits, "head_bits": head_bits})
    return outputs


def render_comparison(cases):
    folder = e.ROOT / "data/faceocc2"
    gt = np.loadtxt(folder / "gt.txt", delimiter=",").astype(np.float32)
    gt[:, :2] -= 1
    rows = {name: json.loads((STUDY / "faceocc2" / f"{name}_frames.json").read_text()) for name in cases}
    cap = cv2.VideoCapture(str(folder / "video.webm"))
    _, first = cap.read()
    height, width = first.shape[:2]
    out = STUDY / "comparison.mp4"
    writer = cv2.VideoWriter(str(out), cv2.VideoWriter_fourcc(*"avc1"), 25, (width * len(cases), height))
    if not writer.isOpened():
        raise RuntimeError("Cannot create comparison video")
    for i in range(1, len(gt)):
        ok, frame = cap.read()
        if not ok:
            raise ValueError("Truncated comparison video")
        panels = []
        for name in cases:
            row = rows[name][i - 1]
            panel = frame.copy()
            for box, color in [(gt[i], (0, 220, 0)), (row["box"], (0, 170, 255))]:
                x, y, w, h = np.round(box).astype(int)
                cv2.rectangle(panel, (x, y), (x + w, y + h), color, 2)
            cv2.rectangle(panel, (0, 0), (width, 28), (20, 20, 20), -1)
            cv2.rectangle(panel, (0, height - 28), (width, height), (20, 20, 20), -1)
            cv2.putText(panel, name, (6, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.48, (255, 255, 255), 1, cv2.LINE_AA)
            cv2.putText(panel, f"Frame {i}   IoU {row['iou']:.2f}", (6, height - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
            panels.append(panel)
        combined = np.concatenate(panels, axis=1)
        writer.write(combined)
        if i in [200, 400, 600, 811]:
            cv2.imwrite(str(STUDY / f"comparison_{i:04d}.jpg"), combined)
    writer.release()
    cap.release()


def main():
    STUDY.mkdir(exist_ok=True)
    plan = {"calibration": "david", "validation": "dudek", "final_evaluation": "faceocc2",
            "selection_metric": "dudek mean IoU, select best all-INT8 candidate",
            "candidates": ["kl128", "minmax128", "mse128"],
            "diagnostic_cases": ["fp32", "baseline8", "float_backbone_int8_head", "int8_backbone_float_head"],
            "note": "Faceocc2 was used in the initial experiment, but is not used to select this round's settings."}
    e.write_json(STUDY / "plan.json", plan)
    fp = paths(e.ROOT / "prepared", ".onnx")
    base = paths(e.ROOT / "quantized", "_int8.onnx")
    cases = {"fp32": fp, "baseline8": base,
             "float_backbone_int8_head": {**fp, "head": base["head"]},
             "int8_backbone_float_head": {**base, "head": fp["head"]}}
    validation = {}
    for name, model_paths in cases.items():
        validation[name] = score_case(name, model_paths, "dudek")
        e.write_json(STUDY / "validation.json", validation)
    pairs = e.calibration(128, STUDY / "calibration_manifest.json", strategy="cached")
    for algorithm in ["kl", "minmax", "mse"]:
        name = algorithm + "128"
        print(f"QUANTIZE {name}", flush=True)
        cases[name] = quantize_case(name, pairs, algorithm)
        validation[name] = score_case(name, cases[name], "dudek")
        e.write_json(STUDY / "validation.json", validation)
    eligible = ["baseline8", *plan["candidates"]]
    best = max(eligible, key=lambda name: validation[name]["mean_iou"])
    selection = {"selected": best, "basis": "dudek only", "validation_mean_iou": validation[best]["mean_iou"]}
    e.write_json(STUDY / "selection.json", selection)
    # The choice is frozen above before looking at any Faceocc2 results.
    final = {}
    for name in dict.fromkeys(["fp32", "baseline8", best]):
        final[name] = score_case(name, cases[name], "faceocc2")
        e.write_json(STUDY / "final_evaluation.json", final)
    render_comparison(list(final))
    print(json.dumps({"selection": selection, "final": final}, indent=2), flush=True)


def precision_study():
    """Keep the original calibration samples; change only branch precision."""
    STUDY.mkdir(exist_ok=True)
    e.write_json(STUDY / "precision_plan.json", {
        "calibration": "Original David 32 local pairs", "validation": "dudek",
        "final_evaluation": "faceocc2 after selection",
        "candidates": {"head16": [8, 16], "backbone16": [16, 8], "full16": [16, 16]},
        "selection_metric": "Highest Dudek mean IoU, retaining baseline8 as a candidate",
    })
    pairs = e.calibration(32, STUDY / "precision_calibration_manifest.json", strategy="local")
    validation = json.loads((STUDY / "validation.json").read_text())
    cases = {"baseline8": paths(e.ROOT / "quantized", "_int8.onnx")}
    for name, bb, head in [("head16", 8, 16), ("backbone16", 16, 8), ("full16", 16, 16)]:
        print(f"QUANTIZE {name}", flush=True)
        cases[name] = quantize_case(name, pairs, head_bits=head, backbone_bits=bb, strategy="local")
        validation[name] = score_case(name, cases[name], "dudek")
        e.write_json(STUDY / "validation.json", validation)
    best = max(cases, key=lambda name: validation[name]["mean_iou"])
    selection = {"selected": best, "basis": "dudek only", "validation_mean_iou": validation[best]["mean_iou"],
                 "best_all_int8": "baseline8"}
    e.write_json(STUDY / "precision_selection.json", selection)
    final = json.loads((STUDY / "final_evaluation.json").read_text())
    if best not in final:
        final[best] = score_case(best, cases[best], "faceocc2")
    e.write_json(STUDY / "final_evaluation.json", final)
    render_comparison(list(final))
    print(json.dumps({"selection": selection, "final": final}, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--precision", action="store_true", help="Run after the all-INT8 study")
    args = parser.parse_args()
    precision_study() if args.precision else main()
