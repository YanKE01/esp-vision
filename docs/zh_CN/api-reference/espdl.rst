espdl -- 模型推理
=================

:link_to_translation:`en:[English]`

``espdl`` 模块在采集到的图像上运行 ESP-DL 的 ``.espdl`` 模型，并提供常见任务封装：目标检测（:py:class:`PedestrianDetect`\ 、:py:class:`ESPDet`\ 、:py:class:`YOLO11`\ ）、姿态估计（:py:class:`YOLO11nPose`\ ）和图像分类（:py:class:`ImageNetCls`\ ），同时可通过 :py:class:`Model` 将 ESP-DL 原始输出 tensor 暴露给 Python。

触摸框选单目标追踪
------------------

``espdl.NanoTrack`` 运行 NanoTrack V2 全 INT8 模型。将 ``models/nanotrack_v2/`` 中的三个 ``.espdl`` 文件复制到设备的 ``/sdcard/nanotrack_v2/`` 即可，也可以通过 ``experiments/nanotrack-v2`` 重新生成。P4/S31 使用 ESP32-P4 导出物；本封装不接受混合 INT16 实验模型。S3 需要面向 S3 重新量化并单独验证。

.. code-block:: python

   import espdl, sensor

   tracker = espdl.NanoTrack(
       "/sdcard/nanotrack_v2/template_deploy.espdl",
       "/sdcard/nanotrack_v2/search_deploy.espdl",
       "/sdcard/nanotrack_v2/head_deploy.espdl",
   )
   try:
       tracker.init(sensor.snapshot(), (80, 60, 64, 64))
       while True:
           img = sensor.snapshot()
           result = tracker.track(img, threshold=0.5)
           if result is not None:
               x, y, w, h, score = result
               img.draw_rectangle(x, y, w, h, color=(0, 255, 0))
           img.flush()
   finally:
       tracker.deinit()

``init(image, roi)`` 缓存目标特征，然后释放模板模型；搜索模型和 head 常驻，重新框选时临时重载模板模型。同一次追踪使用尺寸不变、未绘制标注的 RGB565 或灰度图像。结果为 ``(x, y, w, h, score)``；低于阈值时返回 ``None``，不更新上一搜索状态。分数不能保证目标仍然可见。这是局部单目标追踪，不包含全局自动找回或网络权重训练。

``example/03-Machine-Learning/00-ESP-DL/nanotrack_touch.py`` 在拖动时冻结当前显示图像，忽略留黑区域的初始触摸，将 LCD 坐标映射回相机图像，并允许重新框选。需要启用触摸的板卡；ESP32-P4X-EYE 可运行追踪 API，但没有配置触摸控制器。选区至少为 10x10 相机像素。推理在绘制标注前完成，避免图形进入模型输入。

原始输出 tensor
---------------

.. code-block:: python

   import sensor, espdl

   model = espdl.Model("/sdcard/custom.espdl", mean=(0, 0, 0), std=(255, 255, 255), letterbox=True)
   try:
       print("inputs:", model.inputs())
       print("outputs:", model.outputs())
       img = sensor.snapshot()
       outputs = model.predict(img)
       for name, tensor in outputs.items():
           _, shape, dtype, exponent, raw = tensor
           print(name, shape, dtype, exponent, len(raw))
   finally:
       model.deinit()

当内置任务封装不匹配模型输出布局时，可使用 :py:class:`Model`\ 。ESP-DL 仍负责图像预处理和推理；``predict()`` 返回原始输出 tensor，Python 代码再执行模型相关解码。``inputs()`` 和 ``outputs()`` 返回 ``TensorInfo`` 元组 ``(name, shape, dtype, exponent)``。``predict()`` 返回按输出 tensor 名称索引的 ``RawTensor`` 元组 ``(name, shape, dtype, exponent, bytes)``。应根据 ``dtype`` 解包原始字节，应用 ESP-DL 的二次幂 ``exponent`` 量化比例，再执行 sigmoid、框解码、NMS、softmax 或 top-k 选择等逻辑。若启用 ``letterbox=True``，将坐标映射回源图像前需要先去除模型输入中的 padding。

目标检测
--------

.. code-block:: python

   import sensor, espdl

   det = espdl.ESPDet("/sdcard/espdet_pico_224_224_face.espdl", score=0.5, nms=0.7)
   try:
       img = sensor.snapshot()
       for x, y, w, h, score, category in det.detect(img):
           img.draw_rectangle(x, y, w, h, color=(255, 0, 0), thickness=2)
           img.draw_string(x, max(0, y - 12), "%.2f:%d" % (score, category))
   finally:
       det.deinit()

``score`` 用于过滤低置信度候选框，``nms`` 用于控制重叠检测框的抑制。检测坐标会映射回输入图像，可直接用于绘图。模型应加载一次并在多帧之间复用；``deinit()`` 会释放模型权重和中间缓冲区。

PicoDet
-------

PicoDet 是仓库自带行人模型采用的检测器家族。它的 anchor-point 输出 tensor 由 ESP-DL 的 Pico 后处理器解码，输出布局与 ESPDet、YOLO11 不同。

PedestrianDetect
----------------

:py:class:`PedestrianDetect` 是仓库自带 224x224 PicoDet 行人模型的 Python API。它使用该模型的预处理默认值，并返回标准检测元组：

.. code-block:: python

   det = espdl.PedestrianDetect(
       "/sdcard/pedestrian_detect/pedestrian_detect_pico.espdl",
       score=0.7,
       nms=0.5,
   )

图像分类
--------

.. code-block:: python

   import espdl, image

   img = image.Image("/sdcard/cat.jpg").to_rgb565(copy=True)
   classifier = espdl.ImageNetCls(
       "/sdcard/imagenet_cls_mobilenetv2_s8_v1.espdl",
       topk=5,
       score=0.0,
   )
   try:
       for label, score in classifier.classify(img):
           print(label, "%.4f" % score)
   finally:
       classifier.deinit()

分类结果按照模型输出顺序返回最多 ``topk`` 个 ``(label, score)`` 元组。必要时应将文件图像转换为 RGB565，确保输入格式可被封装层接受。

姿态估计
--------

.. code-block:: python

   pose = espdl.YOLO11nPose("/sdcard/espdet_yolo11n_pose_160_160_coco.espdl", score=0.35, topk=5)
   try:
       img = sensor.snapshot()
       for x, y, w, h, score, category, keypoints in pose.detect(img):
           img.draw_rectangle(x, y, w, h, color=(255, 0, 0))
           for px, py in keypoints:
               if px > 0 and py > 0:
                   img.draw_circle(px, py, 2, color=(0, 255, 0), fill=True)
   finally:
       pose.deinit()

每个姿态结果包含 17 个 COCO 关键点。缺失或低置信度关键点会返回为 ``(0, 0)``，绘图或计算关节几何关系前应将其跳过。

运行时调整阈值
--------------

.. code-block:: python

   det.set_thresholds(score=0.65, nms=0.6)

无需重新加载模型即可修改阈值，适合应用根据光照、距离或场景目标密度的变化进行动态调整。

结果元组
--------

- TensorInfo：``(name, shape, dtype, exponent)``
- RawTensor：``(name, shape, dtype, exponent, bytes)``
- 检测：``(x, y, w, h, score, category)``
- 姿态：``(x, y, w, h, score, category, keypoints)``\ ，含 17 个 COCO 关键点
- 分类：``(label, score)``

.. seealso::

   :doc:`../concepts/ai-inference` 介绍 ESP-DL 推理流程、``.espdl`` 格式、量化以及 前后处理。部署新模型请参见 :doc:`../how-to/add-model`\ 。

   可运行示例：``example/03-Machine-Learning/00-ESP-DL``\ （行人检测、ESP-DL 原始输出解码、ESPDet、YOLO11、姿态、ImageNet 分类）。

.. include:: _generated/espdl.rst
