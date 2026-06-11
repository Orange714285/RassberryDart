# ImageStreamer 实时画面回传使用指南

## 一句话说明

树莓派上跑 `detect_and_draw_lights` 的每一帧画面，**实时显示在你的 Ubuntu 屏幕上**，方便你调试视觉识别效果。

---

## 架构

```
树莓派 (发送端)                        Ubuntu (接收端)
─────────────                          ────────────
./Dart_visual                          python3 stream_receiver.py
    │                                      │
    ├─ 采集相机帧                           ├─ 监听 8080 端口
    ├─ detect_and_draw_lights(frame)       ├─ 接收 JPEG 帧
    ├─ detector_streamer.send(frame)  ────► ├─ cv::imshow 显示
    └─ recorder.write_frame(frame)          └─ 按 ESC 退出
```

---

## 步骤 0：写死 Ubuntu IP（只需改一次）

打开 `lib/src/image_streamer.cpp`，修改顶部调参区第一行：

```cpp
// ============================================================================
// ★ 调参区 —— 所有可调参数集中在这里
// ============================================================================

static constexpr const char* DEFAULT_STREAM_HOST = "192.168.1.100";  // ← 改成你的 Ubuntu IP
static constexpr int         DEFAULT_STREAM_PORT = 8080;             // TCP 端口
static constexpr int         JPEG_QUALITY        = 70;               // JPEG 质量 (0-100)
static constexpr size_t      MAX_FRAME_BYTES     = 10 * 1024 * 1024; // 单帧最大字节
```

然后编译即可。之后树莓派上直接 `./Dart_visual` 就能回传。

> **临时覆盖**：如果某次 Ubuntu IP 变了，用环境变量覆盖：
> ```bash
> STREAM_HOST=192.168.1.200 ./Dart_visual
> ```

---

## 步骤 1：Ubuntu 端启动接收窗口

```bash
cd ~/pi-workspace/LightDetect
python3 stream_receiver.py          # 默认端口 8080
python3 stream_receiver.py 9999     # 自定义端口
```

看到 `[Receiver] 监听端口 8080，等待树莓派连接...` 即可。

---

## 步骤 2：树莓派端启动

```bash
# 直接运行（IP 已在步骤 0 写入 .cpp）
./Dart_visual
```

---

## 步骤 3：观察效果

Ubuntu 上弹出 `LightDetect - Remote View` 窗口，实时显示检测画面，按 `ESC` 退出。

---

## 故障排查

| 现象 | 可能原因 | 解决方法 |
|------|---------|---------|
| Ubuntu 收不到画面 | 防火墙拦截 | `sudo ufw allow 8080/tcp` |
| 连接被拒绝 | 接收脚本未启动 | 先执行步骤 1 |
| 树莓派 `connect() failed` | IP 不对或网络不通 | `ping Ubuntu的IP` |
| 窗口不刷新 | OpenCV GUI 后端问题 | `python3 -c "import cv2; cv2.imshow('t', __import__('numpy').zeros((1,1))); cv2.waitKey(1)"` |
| 画面卡顿 | WiFi 带宽不足 | 有线网络或降低 `JPEG_QUALITY` |
| 不想要回传 | — | 删掉 `detector_streamer.send(frame);` 这一行，重新编译 |
