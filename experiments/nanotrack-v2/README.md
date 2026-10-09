# NanoTrack V2 本地量化实验

2026-10-08：已跑通官方 ONNX → ESP-PPQ ESP32-P4 INT8 校准 → `.espdl` 导出 → PC ONNX Runtime Q/DQ 推理，并完成独立视频的逐帧跟踪比较。实验位于独立目录 `experiments/nanotrack-v2/`。脚本、说明、依赖锁和源文件校验清单可纳入版本管理；虚拟环境、下载数据、模型和运行产物由目录内的 `.gitignore` 排除。没有修改固件。

## 精度改进实验

迁移目录后，新增 `tune.py`，先在另一段 `dudek` 视频（1145 帧，首帧初始化）上选择方案，再回到原 `faceocc2` 视频复测。本轮选择没有使用 `faceocc2` 指标；但该视频在首轮实验中已看过，因此不应宣称这是完全未见的最终测试集。所有校准仍只使用 `david`，未修改后处理参数，也没有在线更新网络权重。

| 方案 | 校准数据 / 方法 | Dudek 平均 IoU |
| --- | --- | ---: |
| FP32 参考 | 不量化 | 0.872642 |
| 原全 INT8 | 32 组邻近模板 / KL | 0.866415 |
| 全 INT8，扩大校准 | 128 组，半数使用首帧模板 / KL | 0.858438 |
| 全 INT8，扩大校准 | 同上 / MinMax | 0.830862 |
| 全 INT8，扩大校准 | 同上 / MSE | 0.858134 |
| Backbone INT8，Head INT16 | 原 32 组 / INT8 用 KL，INT16 用 MinMax | 0.873106 |
| **Backbone INT16，Head INT8** | **原 32 组 / INT16 用 MinMax，INT8 用 KL** | **0.881765** |
| 全 INT16 | 原 32 组 / MinMax | 0.873266 |

单纯增加校准样本或更换方法，本轮没有超过原全 INT8。按 Dudek 平均 IoU 选出的混合方案是 `backbone16`：模板和搜索 backbone 使用 W16A16，head 保持 W8A8，并使用对应 backbone 的输出重新校准 head。不是把整个模型换成 INT16，也不是 ESP-DL 新版的 W8A16 模式。

| Faceocc2 复测指标，811 帧 | FP32 | 原全 INT8 | Backbone INT16 + Head INT8 |
| --- | ---: | ---: | ---: |
| 平均 IoU | 0.696662 | 0.625690 | **0.660839** |
| IoU ≥ 0.5 的帧占比 | 95.19% | 87.67% | **90.88%** |

混合方案平均 IoU 比原 INT8 增加 0.035，恢复约一半量化损失，仍低于 FP32。它在 Dudek 上略高于 FP32，只能视为该视频上量化扰动与跟踪状态累积的结果，不能解释为普遍超过浮点模型。

- [三路对比视频](study/comparison.mp4)：左 FP32，中原全 INT8，右 `backbone16`；绿色为标注，橙色为预测。
- [验证集全部结果](study/validation.json)、[混合方案选择记录](study/precision_selection.json)、[最终复测结果](study/final_evaluation.json)。
- 混合模型：`study/backbone16/template.espdl`（558368 字节）、`study/backbone16/search.espdl`（550624 字节）、`study/backbone16/head.espdl`（242000 字节）。三文件共 1350992 字节（约 1.29 MiB），比原全 INT8 的约 0.86 MiB 增大约 50%。这是文件大小，不是运行内存。
- PC 中 INT16 分支通过 ESP-PPQ CPU 模拟运行，INT8 head 通过 ONNX Runtime Q/DQ 运行。板端 INT16 吞吐和内存代价尚未测量，不能沿用全 INT8 的帧率预估；跨分支张量还需要正确处理布局和 exponent 重定标。
- 原全 INT8 继续保留为速度优先候选，混合方案是本次精度优先候选。进一步提升需要更多物体类别、遮挡和尺度变化数据，再研究敏感层级的局部 INT16 或量化感知训练；不能凭这两段人脸视频做通用精度承诺。

复现实验需先完成下文基线流程，再在 `experiments/nanotrack-v2/` 目录执行：

```sh
uv run --locked tune.py
uv run --locked tune.py --precision
```

本轮还修正了多输入模型接口：ESP-PPQ 的导出图可能重排 input/output 列表，脚本现在按 `input1`、`input2`、`output1`、`output2` 显式绑定，避免模板和搜索特征或两种输出被互换。各 INT8 候选导出均通过 ONNX checker、CPU 执行及三组输入的 PPQ 数值对齐。

## 环境

- 本次验证环境为 macOS arm64、Python 3.11.15，全程 CPU，无需 CUDA。`.python-version` 指定 Python 3.11，`uv run` 自动创建或同步本目录的 `.venv/`，无需手动激活。
- ESP-PPQ 1.3.11、PyTorch 2.5.1、ONNX 1.17.0、ONNX Runtime 1.23.2、OpenCV 4.11.0.86。
- `pyproject.toml` 声明依赖，`uv.lock` 锁定解析结果；使用 `uv run --locked` 保证运行时不改写锁文件。`requirements.lock.txt` 保留首轮实验的 40 个已安装包版本快照，新流程由 uv 项目管理依赖。

## 模型与数据

- 原始模型：HonglinChu/SiamTrackers 的 NanoTrack V2，固定提交 `248663fde6bf7c40190cf10ee396d5662919ecd3`。原始文件在 `models/`，不修改；源地址和 SHA-256 在 `sources.json`。
- `prepared/template.onnx`：将原 backbone 的静态输入改为 `1×3×127×127` 并重新推断形状，输出 `1×48×8×8`。
- `prepared/search.onnx`：输入 `1×3×255×255`，输出 `1×48×16×16`。
- `prepared/head.onnx`：双输入模板/搜索特征；输出 `1×2×16×16` 分类 logits 和 `1×4×16×16` 框距离。
- 校准：OpenCV extra 托管的 OTB `david` 视频，32 组真实模板/搜索裁剪，固定随机种子，加入轻微中心偏移。头部使用量化 backbone 生成的特征校准。
- 独立验证：`faceocc2`，共 812 帧；第 1 帧初始化，后续 811 帧计分。该视频没有参与校准，也没有用于选择或优化量化参数。
- `david` 视频有 770 帧，标注仅有 471 行，从视频第 300 帧开始。脚本跳过前 299 帧，并将 OTB 一基坐标转换为零基坐标。两段视频均检查了帧数与标注数匹配，初始化框已人工目视核验。
- 输入预处理使用 BGR、0～255 浮点值、双线性缩放和均值越界填充；未套用 ImageNet mean/std。跟踪状态和后处理采用 NanoTrack V2 配置：stride 16、window influence 0.490、penalty 0.150、learning rate 0.385。

## 首轮结果

| 指标 | FP32 ONNX Runtime | INT8 ONNX Runtime Q/DQ |
| --- | ---: | ---: |
| 相对人工标注的平均 IoU | 0.696662 | 0.625690 |
| IoU ≥ 0.5 的帧占比 | 95.19% | 87.67% |

平均 IoU 下降约 0.071；这套最小校准集已能跑通，但有可见的精度损失。结果只代表这一段人脸遮挡视频，不能当作通用跟踪精度。两个跟踪器都仅由首帧人工框初始化，后续各自更新状态，人工标注只用于计算指标，没有逐帧重置目标。

- [ONNX Runtime 对比视频](evaluation_qdq/comparison.mp4)：左 FP32，右 INT8；绿色为人工标注，橙色为预测框。
- [结果摘要](evaluation_qdq/summary.json) 和 [逐帧结果](evaluation_qdq/frames.json)。
- [ESP-PPQ CPU 模拟对比视频](evaluation/comparison.mp4) 和 [模拟结果](evaluation/summary.json)。两种量化执行路径在本视频上的逐帧预测框一致，详见 `verification.json`。

CPU 时间只描述这台 Mac 上的执行，不能用于推断 ESP32 帧率。PC Q/DQ 和 PPQ 模拟均不等同于实际运行 `.espdl`：尚未检查板端算子、LUT、内存、时延及数值一致性。项目固件固定 ESP-DL 3.3.6，本实验使用 ESP-PPQ 1.3.11；与该固件的完整兼容性尚待上板验证。

## 导出文件

| 分支 | 不含测试张量的 ESP-DL 模型 | 字节数 | PC INT8 ONNX |
| --- | --- | ---: | --- |
| 模板 | `quantized/template_deploy.espdl` | 328096 | `quantized/template_int8.onnx` |
| 搜索 | `quantized/search_deploy.espdl` | 328096 | `quantized/search_int8.onnx` |
| 跟踪头 | `quantized/head_deploy.espdl` | 241728 | `quantized/head_int8.onnx` |

三个部署文件共 897920 字节（约 877 KiB），模板与搜索文件包含重复的 backbone 权重；这不是运行内存大小。`quantized/{template,search,head}.espdl` 另含测试输入输出，供后续板端数值对齐；`.native` 保存供本地 PPQ 执行的量化图，`.json` 和 `.info` 保存配置与布局信息。

PC 导出时发现 ESP-PPQ 的 ONNX Runtime exporter 固定输出 opset 13，但 backbone 的 HardSwish 要求 opset ≥14。脚本仅对导出的 Q/DQ 图修正为 opset 14，并执行 ONNX checker 与 ONNX Runtime 验证，未修改安装包。Exporter 对部分 INT32 bias 发出通用的非 8 位警告；实际三个模型均能加载和执行，单组相同输入下与 PPQ 的最大输出误差均为 0，整段视频也已交叉验证。原始浮点 ONNX、ESP-DL 和 PC Q/DQ 导出物分别保留。

## 复现

先从仓库根目录进入实验目录，再直接使用 `uv run`。首次运行会自动准备 Python 环境并安装锁定的依赖；无需单独执行 `uv venv`、`uv pip sync` 或调用 `.venv/bin/python`。`fetch_assets.py` 会下载缺失资产，并校验已有文件的 SHA-256。

```sh
cd experiments/nanotrack-v2
export UV_CACHE_DIR="$PWD/.cache/uv"

uv run --locked fetch_assets.py
uv run --locked experiment.py prepare
uv run --locked experiment.py quantize --calibration-count 32
uv run --locked experiment.py export
uv run --locked experiment.py evaluate --frames 812 --int8-backend onnx
uv run --locked experiment.py evaluate --frames 812 --int8-backend ppq
```

也可以留在仓库根目录，通过 `--directory` 指定实验目录，例如 `uv run --directory experiments/nanotrack-v2 --locked experiment.py prepare`。

`quantize.log`、`export.log`、`evaluate.log` 和 `evaluate_qdq.log` 保留本次执行记录。重跑会覆盖当前实验输出。

## 来源

- [NanoTrack V2 模型](https://github.com/HonglinChu/SiamTrackers/tree/248663fde6bf7c40190cf10ee396d5662919ecd3/NanoTrack/models/nanotrackv2)：NanoTrack 子目录包含 Apache-2.0 LICENSE，本地保留为 `models/UPSTREAM_LICENSE`。全 INT8 部署副本已放到仓库的 `models/nanotrack_v2/`，附有来源、SHA-256 和上游许可证文本；权重和训练数据的再分发条件尚未单独核实。
- [OpenCV tracking 测试数据说明](https://github.com/opencv/opencv_extra/blob/5.x/testdata/cv/tracking/README.md)：原始序列和标注来自 Wu、Lim、Yang 的 CVPR 2013 Online Object Tracking Benchmark。下载固定在 `sources.json` 记录的提交，仅作为本地校准和验证数据。
- [ESP-PPQ](https://github.com/espressif/esp-ppq)：通过 uv 安装；量化目标为 ESP32-P4 w8a8，使用 ESP-PPQ 默认 KL 校准策略。
