# Light Detect - 项目架构说明

## 项目结构

```
Light_detect_final_2/
├── CMakeLists.txt              # 顶层 CMake 配置文件
├── CMakeLIsts.txt.bak          # 原始 CMake 配置备份
├── main.cpp.bak                # 原始 main.cpp 备份
├── include/                    # 头文件目录
│   ├── one_euro_filter.hpp    # OneEuroFilter 类头文件
│   └── detector.hpp           # Detector 类头文件
└── src/                        # 源文件目录
    ├── CMakeLists.txt         # src 子目录的 CMake 配置
    ├── main.cpp               # 主程序（libcamera 相关）
    └── detector.cpp           # Detector 类实现
```

## 模块说明

### 1. OneEuroFilter (`include/one_euro_filter.hpp`)
- **功能**: 1-Euro 滤波器实现
- **用途**: 对检测到的灯光坐标进行平滑处理，减少抖动
- **参数**:
  - `min_cutoff`: 最小截止频率 (默认 0.5Hz)
  - `beta`: 速度系数 (默认 0.0)
  - `d_cutoff`: 导数截止频率 (默认 1.0Hz)

### 2. Detector (`include/detector.hpp` & `src/detector.cpp`)
- **功能**: 灯光检测与追踪
- **主要方法**: `detect_and_draw_lights(cv::Mat &frame)`
- **检测流程**:
  1. 颜色阈值分割（绿色光检测）
  2. 形态学处理
  3. 轮廓检测与筛选
  4. OneEuroFilter 平滑
  5. 结果输出到 CSV 文件
- **输出文件**: `data.csv`

### 3. Main (`src/main.cpp`)
- **功能**: libcamera 相机接口与主控制逻辑
- **核心功能**:
  - libcamera 相机初始化与配置
  - 帧捕获与格式转换
  - 多线程队列管理
  - 统计信息输出
- **保留内容**: 所有 libcamera 相关代码

## 编译与运行

### 编译

```bash
cd /home/orange/RP_Working/Light_Detect/Light_detect_final_2
mkdir build && cd build
cmake ..
make
```

### 运行

```bash
./LightDetect
```

### 退出

按 `Ctrl+C` 优雅退出，程序会自动：
- 关闭相机
- 释放所有资源
- 关闭 CSV 文件

## 参数配置

在 `src/main.cpp` 中可配置的参数：

```cpp
static constexpr int      kWidth  = 640;      // 图像宽度
static constexpr int      kHeight = 480;      // 图像高度
static constexpr int32_t  kExposureUs = 1000; // 曝光时间（微秒）
static constexpr float    kAnalogueGain = 1.0f; // 模拟增益
static constexpr int      kTargetFps = 60;    // 目标帧率
static constexpr unsigned kBufferCount = 8;   // buffer 数量
static constexpr int      kQueueMax = 180;    // 最大队列长度
```

在 `src/detector.cpp` 中可配置的颜色阈值：

```cpp
int rmin = 0, rmax = 255;    // 红色通道范围
int gmin = 170, gmax = 255;  // 绿色通道范围（检测绿色光）
int bmin = 0, bmax = 255;    // 蓝色通道范围
```

## 输出说明

### 控制台输出
```
CAP_FPS: 60 | DETECT_FPS: 60 | total_cap: 3600 | total_drop: 0 | total_detect: 3600 | qlen: 5 | exp(us): 1000
```
- `CAP_FPS`: 相机捕获帧率
- `DETECT_FPS`: 检测处理帧率
- `total_cap`: 总捕获帧数
- `total_drop`: 总丢帧数
- `total_detect`: 总检测帧数
- `qlen`: 当前队列长度
- `exp(us)`: 曝光时间（微秒）

### CSV 输出 (data.csv)
```csv
Index,x,y,oneEuro_x,oneEuro_y,frame_dtMs
1,320,240,320.5,240.3,16
2,321,241,320.8,240.5,17
...
```
- `Index`: 帧序号
- `x, y`: 原始检测坐标
- `oneEuro_x, oneEuro_y`: 滤波后坐标
- `frame_dtMs`: 帧间隔（毫秒）

## 依赖

- CMake >= 3.10
- C++17
- libcamera
- OpenCV4
- 树莓派平台（支持 OV5647 相机）

## 架构优势

1. **模块化**: 功能分离，便于维护和扩展
2. **现代 CMake**: 使用子目录结构，符合最佳实践
3. **封装性**: 类实现独立，降低耦合
4. **可读性**: 代码结构清晰，易于理解
5. **可扩展**: 方便添加新的检测算法或功能模块
