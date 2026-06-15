# Tools — 性能分析工具集

> 这种性能分析工具我完全交给AI生成了

所有工具均为 **header-only、全静态方法**，无需实例化，`#include` 即用。

---

## 1. Timer — 代码耗时测量

**文件**：`lib/include/tools/timer.hpp`

### 风格 A：手动 start / stop

```cpp
#include <tools/timer.hpp>

Timer::start("detect_and_draw_lights");
detector.detect_and_draw_lights(frame);
Timer::stop();
// 输出: [Timer] detect_and_draw_lights: 3.2 ms
```

- `start(name)` — 开始计时。若已处于计时状态则报错（不支持嵌套）。
- `stop()` — 停止计时并打印耗时（毫秒）。

### 风格 B：measure lambda（推荐）

```cpp
Timer::measure("streamer.send", [&]{
    streamer.send(frame);
});
// 输出: [Timer] streamer.send: 1.5 ms
```

- 自动计时，自动输出，不可能出现配对错误。
- **支持嵌套**——每层 measure 独立计时。

---

## 2. CpuMonitor — CPU 占用率监控

**文件**：`lib/include/tools/cpu_monitor.hpp`

通过读取 `/proc/stat` 计算整机 CPU 占用率。

### 基本用法

```cpp
#include <tools/cpu_monitor.hpp>

CpuMonitor::sample();   // 首次采样（建立基准）

while (g_running)
{
    CpuMonitor::sample();                    // 每帧采样
    double cpu_pct = CpuMonitor::usage();    // 获取占用率 (0~100)

    // ... 主循环逻辑 ...
}
```

### 导出 CSV

```cpp
CpuMonitor::log_to_csv("cpu.csv");
// 写入文件: 20260611_173000_cpu.csv
// 内容追加: 2026-06-11 17:30:00, 23.5%
```

---

## 3. FrameCounter — FPS 帧率统计

**文件**：`lib/include/tools/frame_counter.hpp`

### 用法

```cpp
#include <tools/frame_counter.hpp>

while (g_running)
{
    FrameCounter::tick();                 // 每帧必须调用

    // ...

    if (FrameCounter::total() % 60 == 0)
        std::cout << "[FPS] " << FrameCounter::fps()
                  << " | total: " << FrameCounter::total() << std::endl;
}
```

| 方法 | 说明 |
|------|------|
| `tick()` | 每帧调用一次 |
| `fps()` | 当前 FPS（基于最近 ~1 秒滑动窗口） |
| `total()` | 启动以来总帧数 |
| `log_to_csv("fps.csv")` | 写入 `YYYYMMDD_HHMMSS_fps.csv` |

### 导出 CSV

```cpp
FrameCounter::log_to_csv("fps.csv");
// 写入文件: 20260611_173000_fps.csv
// 内容追加: 2026-06-11 17:30:00, 59.8
```

---

## 完整示例（集成到 main.cpp）

```cpp
#include <tools/cpu_monitor.hpp>
#include <tools/frame_counter.hpp>
#include <tools/timer.hpp>

int main()
{
    CpuMonitor::sample();   // 建立基准

    while (g_running)
    {
        CpuMonitor::sample();
        FrameCounter::tick();

        cv::Mat frame = ov5647.wait_and_get_latest_frame();

        Timer::measure("detect", [&]{
            detector.detect_and_draw_lights(frame);
        });

        if (FrameCounter::total() % 60 == 0)
        {
            std::cout << "[FPS] " << FrameCounter::fps()
                      << " | CPU: " << CpuMonitor::usage() << "%" << std::endl;
            CpuMonitor::log_to_csv("cpu.csv");
        }
    }
}
```

---

## 目录结构

```
lib/include/tools/
├── README.md
├── timer.hpp
├── cpu_monitor.hpp
└── frame_counter.hpp
```

## 注意事项

| | Timer | CpuMonitor | FrameCounter |
|---|---|---|---|
| 平台 | Linux | Linux（`/proc/stat`） | Linux |
| 精度 | 微秒级 | ~10ms | ~1s 滑动窗口 |
| 开销 | 近乎零 | 读文件 (~微秒) | 近乎零 |
| 线程安全 | 否 | 否 | 否 |
