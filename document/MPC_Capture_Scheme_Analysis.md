# Dart 项目 Capture 方案 —— MCAP 数据录制从零实现完全指南

> 基于 `/home/orange/Project/Dart` 项目源码的完整逆向解析，涵盖架构设计、环境配置、实现细节与最小可运行示例。

---

## 目录

1. [整体架构概览](#一整体架构概览)
2. [环境配置](#二环境配置)
3. [核心技术栈详解](#三核心技术栈详解)
4. [关键源码剖析](#四关键源码剖析)
5. [从零实现完整流程](#五从零实现完整流程)
6. [最小可运行示例](#六最小可运行示例)
7. [调试与验证](#七调试与验证)
8. [附录：依赖版本清单](#八附录依赖版本清单)

---

## 一、整体架构概览

### 1.1 数据流管线

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌────────────────┐
│  Sensors    │ -> │   Detect    │ -> │   Track     │ -> │   Capture      │
│  ─────────  │    │  ─────────  │    │  ─────────  │    │  ───────────── │
│  HikRobot   │    │  OpenVINO   │    │  OneEuro    │    │  FlatBuffers   │
│  相机 + 串口 │    │  目标检测    │    │  Filter跟踪  │    │  → MCAP 写入   │
└─────────────┘    └─────────────┘    └─────────────┘    └────────────────┘
                                                               │
                                                    CaptureInfo 结构体
                                                    (CarData + VisionData
                                                     + Timestamp + cv::Mat)
                                                               │
                                                    ① FlatBuffers 序列化
                                                    ② MCAP Message 封装
                                                    ③ 写入 .mcap 文件
```

### 1.2 MCAP 文件内部结构

```
┌──────────────────────────────────────────────────────────────┐
│ Magic (8 bytes)          │ \x89 M C A P 0 \r \n              │
├──────────────────────────────────────────────────────────────┤
│ Header Record            │ profile="flatbuffer"              │
│                          │ library="libmcap 1.3.0"           │
├──────────────────────────────────────────────────────────────┤
│ Data Section             │                                   │
│  ├─ Schema Record        │  name + encoding + BFBS 字节       │
│  ├─ Channel Record       │  topic + encoding + schemaId      │
│  ├─ Chunk #1:            │  ┌─────────────────────────────┐  │
│  │  ├─ Message #1        │  │ channelId, logTime,         │  │
│  │  ├─ Message #2        │  │ publishTime, FlatBuffer数据  │  │
│  │  ├─ ...               │  └─────────────────────────────┘  │
│  │  └─ MessageIndex[N]   │  每个 Channel 的 offset 索引表     │
│  ├─ Chunk #2: ...        │                                   │
│  └─ DataEnd              │  标记 Data Section 结束            │
├──────────────────────────────────────────────────────────────┤
│ Summary Section          │  ChunkIndex, Statistics,          │
│                          │  AttachmentIndex, MetadataIndex   │
├──────────────────────────────────────────────────────────────┤
│ Summary Offset Section   │  各 Summary 记录的文件偏移          │
├──────────────────────────────────────────────────────────────┤
│ Footer Record            │  summaryStart + summaryOffsetStart │
│ Magic (8 bytes)          │  \x89 M C A P 0 \r \n  (尾部魔术)  │
└──────────────────────────────────────────────────────────────┘
```

---

## 二、环境配置

### 2.1 系统要求

- **操作系统**: Ubuntu 22.04 LTS (其他 Linux 发行版同理)
- **编译器**: GCC 11+ / Clang 14+ (需要 C++20 支持)
- **构建系统**: CMake 3.22+

### 2.2 依赖安装

#### 方式一：APT 安装（推荐，快速起步）

```bash
# 基础构建工具
sudo apt update
sudo apt install -y build-essential cmake git pkg-config

# FlatBuffers 编译器和库
sudo apt install -y flatbuffers-compiler flatbuffers-dev

# 压缩库（MCAP Chunk 压缩依赖）
sudo apt install -y liblz4-dev libzstd-dev

# OpenCV（图像处理，如不需要图像可省略）
sudo apt install -y libopencv-dev
```

#### 方式二：源码编译 FlatBuffers（获取最新版本）

APT 仓库的 FlatBuffers 版本可能较旧。本项目使用的是 v25.2.10，推荐从源码编译：

```bash
# 克隆 FlatBuffers
git clone https://github.com/google/flatbuffers.git
cd flatbuffers
git checkout v25.2.10   # 与本项目对齐

# 编译安装
cmake -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DFLATBUFFERS_BUILD_TESTS=OFF \
  -DFLATBUFFERS_BUILD_FLATHASH=OFF \
  -DFLATBUFFERS_BUILD_FLATLIB=OFF \
  -DCMAKE_INSTALL_PREFIX=/usr/local

make -j$(nproc)
sudo make install
sudo ldconfig

# 验证安装
flatc --version
# 输出: flatc version 25.2.10
```

安装后文件位置：

| 文件 | 路径 |
|------|------|
| flatc 编译器 | `/usr/local/bin/flatc` |
| 头文件 | `/usr/local/include/flatbuffers/` |
| 静态库 | `/usr/local/lib/libflatbuffers.a` |
| pkg-config | `/usr/local/lib/pkgconfig/flatbuffers.pc` |

#### 方式三：使用系统包管理器安装 flatc 二进制

```bash
# 如果只需要 flatc 编译器（不编译 C++ 库）
# 可以直接下载预编译二进制
wget https://github.com/google/flatbuffers/releases/download/v25.2.10/Linux.flatc.binary.clang++-18.zip
unzip Linux.flatc.binary.clang++-18.zip
sudo mv flatc /usr/local/bin/
```

### 2.3 MCAP 库集成

本项目的 MCAP 采用 **header-only** 方式内嵌在源码中，位于：

```
src/utility/inc/mcap/
├── crc32.hpp           # CRC32 计算
├── errors.hpp           # 错误处理
├── internal.hpp         # 内部工具函数
├── intervaltree.hpp     # 区间树（读取用）
├── mcap.hpp             # 聚合头文件
├── reader.hpp           # 读取器声明
├── reader.inl           # 读取器实现
├── read_job_queue.hpp   # 异步读取队列
├── types.hpp            # 所有 MCAP 数据类型定义
├── types.inl            # 数据类型内联实现
├── visibility.hpp       # 跨平台符号导出宏
├── writer.hpp           # 写入器声明
└── writer.inl           # 写入器实现（核心）
```

**使用方式**：在需要 MCAP 功能的源文件中：

```cpp
#ifndef MCAP_IMPLEMENTATION
#define MCAP_IMPLEMENTATION   // 必须在 include 之前定义
#endif
#include "mcap/writer.hpp"    // 这会触发 writer.inl 的编译
```

这个 `MCAP_IMPLEMENTATION` 宏的作用是控制 header-only 库的实现体展开——定义后，`.inl` 文件才会被 `#include`，确保整个程序中只编译一次 MCAP 实现。

### 2.4 验证所有依赖

```bash
# 一键检查脚本
echo "=== 编译器 ==="
g++ --version | head -1

echo "=== CMake ==="
cmake --version | head -1

echo "=== flatc ==="
flatc --version

echo "=== FlatBuffers ==="
pkg-config --modversion flatbuffers

echo "=== lz4 ==="
dpkg -l liblz4-dev | tail -1

echo "=== zstd ==="
dpkg -l libzstd-dev | tail -1

echo "=== OpenCV ==="
pkg-config --modversion opencv4 2>/dev/null || echo "未安装(可选)"
```

---

## 三、核心技术栈详解

### 3.1 MCAP 格式

**MCAP** (MCAP is a Container for Automotive and robotics data，发音 "em-cap") 是 Foxglove 开发的开放标准二进制容器格式，专为机器人/自动驾驶数据录制设计。

**核心优势**：

| 特性 | 说明 |
|------|------|
| 自描述 | Schema 嵌入文件，无需外部定义即可解析 |
| 可索引 | MessageIndex + ChunkIndex 支持 O(1) 按时间戳随机访问 |
| 压缩 | 内置 Zstd / LZ4 Chunk 级压缩 |
| 流式写入 | 支持边录边写，crash-safe |
| 多 Channel | 单个文件支持多个独立 Topic |
| 附件 | 支持嵌入任意文件（标定参数、配置文件等） |

**Specification**: https://mcap.dev/spec (当前版本 v1)

### 3.2 FlatBuffers 序列化

FlatBuffers 是 Google 开发的零拷贝序列化库，特点：

- 反序列化无需解析/解包，直接从内存读取
- 支持 Schema 演化（前向/后向兼容）
- 将 Schema 编译为 `.bfbs`（Binary FlatBuffer Schema）实现自描述

**选择 FlatBuffers vs Protobuf 的原因**：
- Protobuf 需要反射或 `.proto` 源文件才能自描述
- FlatBuffers 的 BFBS 是标准化的紧凑二进制格式，元数据开销更小
- 读取时零拷贝，非常适合大图像数据的低延迟场景

### 3.3 BFBS 文件详解

BFBS 是 FlatBuffers 的 self-describing schema 格式。它本质上是一个序列化后的 `reflection::Schema` FlatBuffer：

```bash
# 查看 BFBS 文件头
xxd CaptureInformation.bfbs | head -5
# 00000000: 1c00 0000 4246 4253 ...  ← "BFBS" 魔数
# 00000070: 2f2f 4361 7074 7572 6549 6e66 6f72 6d61  //CaptureInforma...
```

BFBS 内容包含完整的 Schema 元信息：字段名、类型、offset、默认值等。

**编译命令**：

```bash
# 将所有 .fbs 编译为 C++ 头文件 + BFBS 文件
flatc \
  --cpp \                    # 生成 C++ 头文件
  --bfbs-comments \          # BFBS 中保留注释
  --bfbs-builtins \          # BFBS 中展开内建类型
  -o ../autogenerated_flatbuffers/ \   # 输出目录
  *.fbs                      # 输入 schema 文件

# 产物:
#   *_generated.h   → C++ 头文件 (含 CreateXxx, XxxBuilder 等 API)
#   *.bfbs          → 二进制 Schema 文件 (嵌入 MCAP 实现自描述)
```

**重要**：BFBS 文件必须与对应的 `*_generated.h` 使用**同一次 flatc 编译**产生，否则可能存在兼容性问题。

---

## 四、关键源码剖析

### 4.1 项目文件结构

```
Dart/
├── main.cpp                          # 入口，创建线程池执行 pipeline
├── CMakeLists.txt                    # 顶层构建
├── config/
│   ├── flatbuffers/                  # .fbs Schema 源文件
│   │   ├── Time.fbs                  #   时间戳结构
│   │   ├── CarData.fbs              #   车辆状态 (30+ 字段)
│   │   ├── VisionData.fbs           #   视觉检测结果
│   │   ├── RawImage.fbs             #   原始图像 (Foxglove 标准)
│   │   └── CaptureInformation.fbs   #   顶层聚合表
│   ├── autogenerated_flatbuffers/    # flatc 编译产出
│   │   ├── *_generated.h            #   C++ 头文件
│   │   └── *.bfbs                   #   二进制 Schema
│   └── json/
│       └── capture.json             #   录制配置 (帧数/路径)
│
└── src/
    ├── capture/                      # ★ Capture 模块 ★
    │   ├── CMakeLists.txt
    │   ├── inc/Capture.hpp           #   类声明
    │   └── src/Capture.cpp           #   类实现
    │
    └── utility/
        ├── inc/
        │   ├── mcap/                 # ★ MCAP header-only 库 ★
        │   │   ├── writer.hpp        #   McapWriter 声明
        │   │   ├── writer.inl        #   McapWriter 实现
        │   │   ├── reader.hpp        #   McapReader (回放用)
        │   │   ├── reader.inl
        │   │   ├── types.hpp         #   数据结构定义
        │   │   └── types.inl
        │   └── function/function.hpp #   工具函数
        └── src/function/function.cpp #   工具函数实现
```

### 4.2 数据结构定义

```cpp
// Struct.hpp —— 内存中的中间数据结构

// 车辆底盘数据 (串口读取，packed 对齐)
struct CarData {
    uint8_t mode, status, number, dune, yaw_error;
    uint8_t car_ctrl_mode, load_task_step, shoot_task_step;
    uint8_t LauncherTaskState;
    // ... 共 30+ 个字段 ...
} __attribute__((packed));

// 视觉检测结果
struct VisionData {
    float yaw_error;
    uint8_t target_status;  // 0=Lost, 1=Found
} __attribute__((packed));

// 传感器原始帧
typedef struct {
    CarData cd;
    foxglove::Time timestamp;
    cv::Mat img;
} FromData;

// Track 模块输出 —— 也是 Capture 的输入
typedef struct {
    CarData cd;
    VisionData vd;
    foxglove::Time timestamp;
    cv::Mat img;
} CaptureInfo;
```

### 4.3 Capture.hpp 类声明

```cpp
class Capture {
public:
    // 构造函数：指定 topic/frame/schema/encoding
    Capture(std::string topic_name,   // "/Dart"
            std::string frame_id,     // "Vision"
            std::string schema_name,  // "foxglove.CaptureInformation"
            std::string schema_path,  // "path/to/CaptureInformation.bfbs"
            std::string encoding,     // "8UC3"
            std::string mcap_path = "");

    void init();                          // 打开文件 + 注册 Schema/Channel
    void finish();                        // 关闭文件
    void setMacpPath(std::string path);   // 设置输出路径
    void update(CaptureInfo &info);       // 序列化并写入一帧

private:
    mcap::McapWriter writer_;                           // MCAP 写入器
    std::unique_ptr<mcap::Channel> channel;             // 数据通道
    std::unique_ptr<mcap::Schema> schema_;              // Schema 描述
    flatbuffers::FlatBufferBuilder builder_;            // FlatBuffer 构建器
    std::string topic_name_, frame_id_, schema_name_;  // 配置
    std::string schema_path_, mcap_path_, encoding_;
};
```

### 4.4 Capture.cpp 核心实现逐行解析

#### init() —— 初始化阶段

```cpp
void Capture::init() {
    // 步骤 1: 确保输出目录和文件可创建
    function::tryToCreateAimFile(this->mcap_path_);

    // 步骤 2: 打开 MCAP 文件
    //   McapWriterOptions("flatbuffer") 设置 profile
    //   此行触发: 写 Magic bytes + Header Record
    auto status = this->writer_.open(
        this->mcap_path_,
        mcap::McapWriterOptions("flatbuffer")
    );

    // 步骤 3: 读取 BFBS 文件内容
    auto encoding = function::getFileContents(this->schema_path_);

    // 步骤 4: 创建并注册 Schema
    //   参数: name, encoding 类型, data (BFBS 文件字节)
    this->schema_ = std::make_unique<mcap::Schema>(
        this->schema_name_,           // "foxglove.CaptureInformation"
        "flatbuffer",                 // 编码类型
        encoding                      // BFBS 原始字节
    );
    this->writer_.addSchema(*this->schema_.get());
    // ↑ addSchema 会: ①自动分配 schema_->id (uint16_t)
    //                ②向文件写入 Schema Record

    // 步骤 5: 创建并注册 Channel
    this->channel = std::make_unique<mcap::Channel>(
        this->topic_name_,            // "/Dart"
        "flatbuffer",                 // 消息编码类型
        this->schema_->id             // 关联的 Schema ID
    );
    this->writer_.addChannel(*this->channel.get());
    // ↑ addChannel 会: ①自动分配 channel->id (uint16_t)
    //                 ②向文件写入 Channel Record
}
```

#### update() —— 写入每帧数据

```cpp
void Capture::update(CaptureInfo &info) {
    // ═══════════════════════════════════════
    // 阶段一: FlatBuffers 序列化
    // ═══════════════════════════════════════
    this->builder_.Clear();  // 重置构建器（复用内存）

    // 1. 构建子表: CarData
    auto cd = foxglove::CreateCarData(
        this->builder_,
        info.cd.mode, info.cd.status, info.cd.number,
        info.cd.dune, info.cd.yaw_error,
        // ... 30+ 个字段 ...
        info.cd.dart_remaining_time
    );

    // 2. 构建子表: VisionData
    auto vd = foxglove::CreateVisionData(
        this->builder_,
        info.vd.yaw_error,
        info.vd.target_status
    );

    // 3. 构建字符串 (FlatBuffer 中字符串需要单独构造)
    auto encoding  = this->builder_.CreateString(this->encoding_);  // "8UC3"
    auto frame_id  = this->builder_.CreateString(this->frame_id_);  // "Vision"

    // 4. 构建图像数据 vector
    //    cv::Mat::total() = rows*cols  (像素数)
    //    cv::Mat::elemSize() = 每像素字节数 (8UC3 = 3)
    auto data_vec = this->builder_.CreateVector(
        info.img.data,
        info.img.total() * info.img.elemSize()
    );

    // 5. 构建时间戳
    auto timestamp = function::getNowTimestamp();

    // 6. 构建 RawImage 表 (Foxglove 标准图像格式)
    auto raw_image = foxglove::CreateRawImage(
        this->builder_,
        &timestamp,          // 时间戳引用
        frame_id,            // 坐标系
        info.img.cols,       // 宽度
        info.img.rows,       // 高度
        encoding,            // 编码格式
        info.img.step,       // 行步长 (含 padding)
        data_vec             // 像素数据
    );

    // 7. 构建顶层 CaptureInformation 表 (使用 Builder 模式)
    auto capture_info_builder = foxglove::CaptureInformationBuilder(this->builder_);
    capture_info_builder.add_cd(cd);
    capture_info_builder.add_vd(vd);
    capture_info_builder.add_raw_image(raw_image);
    auto capture_info = capture_info_builder.Finish();

    // 8. 完成 FlatBuffer 构建
    this->builder_.Finish(capture_info);
    auto data_ptr  = this->builder_.GetBufferPointer();
    auto data_size = this->builder_.GetSize();

    // ═══════════════════════════════════════
    // 阶段二: MCAP Message 封装与写入
    // ═══════════════════════════════════════

    // 9. 拷贝数据到独立内存 (确保在 MCAP 异步写入期间数据有效)
    std::shared_ptr<std::byte> data_msg(new std::byte[data_size]);
    std::memcpy(data_msg.get(), data_ptr, data_size);

    // 10. 填充 Message 结构
    mcap::Message msg;
    msg.channelId   = this->channel->id;            // 所属 Channel
    msg.sequence    = 1;                            // 序列号 (简化处理)
    msg.logTime     = function::nanosecondsSinceEpoch();  // 记录时间
    msg.publishTime = msg.logTime;                  // 发布时间 = 记录时间
    msg.data        = data_msg.get();               // FlatBuffer 数据指针
    msg.dataSize    = data_size;                    // 数据长度

    // 11. 写入 MCAP 文件
    this->writer_.write(msg);
    // ↑ write() 内部: ①将数据追加到当前 Chunk 缓冲区
    //               ②当缓冲区 >= chunkSize 时，压缩并写入文件
}
```

#### finish() —— 关闭文件

```cpp
void Capture::finish() {
    this->writer_.close();
    // ↑ close() 内部按顺序:
    //   ① closeLastChunk()  —— 将当前未满的 Chunk 压缩写入
    //   ② 写入 DataEnd Record —— 标记数据段结束
    //   ③ 写入 Summary Section —— Statistics, ChunkIndex, ...
    //   ④ 写入 SummaryOffset Section
    //   ⑤ 写入 Footer Record —— 指向 Summary 的偏移量
    //   ⑥ 写入尾部 Magic bytes
    //   ⑦ 刷新文件缓冲区到磁盘
}
```

### 4.5 MCAP Writer 配置项

```cpp
struct McapWriterOptions {
    bool noChunkCRC       = false;    // 不计算 Chunk CRC
    bool noAttachmentCRC  = false;    // 不计算 Attachment CRC
    bool enableDataCRC    = false;    // 计算 Data Section CRC
    bool noSummaryCRC     = false;    // 不计算 Summary CRC
    bool noChunking       = false;    // 不分 Chunk (直接写)
    bool noMessageIndex   = false;    // 不写消息索引
    bool noSummary        = false;    // 不写 Summary 段 (更快/更小)
    uint64_t chunkSize    = 1024*768; // Chunk 目标大小 (解压后字节数)
    Compression compression      = Compression::Zstd;  // 压缩算法
    CompressionLevel compLevel   = CompressionLevel::Default;
    bool forceCompression = false;    // 即使无收益也强制压缩
    std::string profile;              // 必填: "flatbuffer"
    std::string library = "libmcap " MCAP_LIBRARY_VERSION;
};
```

### 4.6 main.cpp 中的生产级调用

```cpp
// 在独立线程中运行录制逻辑
auto capture_func = [&]() -> void {
    // 1. 创建 Capture 实例
    Capture cap(
        "/Dart",                                        // topic
        "Vision",                                       // frame_id
        "foxglove.CaptureInformation",                  // schema name
        "/path/to/CaptureInformation.bfbs",             // schema file
        "8UC3"                                          // image encoding
    );

    // 2. 初始化 (打开文件 + 注册 schema/channel)
    cap.setMacpPath(function::getBagPath());  // "../bag/2025_6_10_15_30_00.mcap"
    cap.init();

    // 3. 从配置读取每文件最大帧数
    int frame_count;
    J_CAPTURE.config_["frame_cnt"] >> frame_count;  // 2000

    // 4. 主循环
    while (true) {
        // 从无锁栈取出 Track 模块输出的数据
        std::optional<CaptureInfo> opt_value = capture_stack.pop();

        // 过滤无效帧
        if (!opt_value.has_value() ||
            opt_value.value().cd.dune == 1 ||     // 沙丘模式
            !opt_value.value().cd.mode) {          // 非自动模式
            continue;
        }

        // 写入一帧
        cap.update(opt_value.value());

        // 5. 轮转录制：每 N 帧切一个文件
        if (!(--frame_count)) {
            cap.finish();                                // 关闭当前 .mcap
            cap.setMacpPath(function::getBagPath());     // 生成新文件名
            cap.init();                                  // 打开新 .mcap
            J_CAPTURE.config_["frame_cnt"] >> frame_count; // 重置计数器
        }
    }
};

// 添加到线程池
t_p.addTask(capture_func);
t_p.run();  // 所有线程并行执行
```

---

## 五、从零实现完整流程

### Step 1: 创建项目骨架

```bash
# 创建目录结构
mkdir -p my-recorder/{src,config/flatbuffers,config/autogenerated_flatbuffers,build}
cd my-recorder

# 下载 MCAP 头文件库
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/writer.hpp
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/types.hpp
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/errors.hpp
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/visibility.hpp
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/internal.hpp
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/crc32.hpp
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/writer.inl
wget https://raw.githubusercontent.com/foxglove/mcap/main/cpp/mcap/include/mcap/types.inl
mkdir -p inc/mcap
mv *.hpp *.inl inc/mcap/
```

### Step 2: 定义数据 Schema

`config/flatbuffers/MyRecord.fbs`:

```flatbuffers
namespace myapp;

struct Time {
    sec:  uint32;
    nsec: uint32;
}

table SensorState {
    sensor_id:  uint32;
    value:      float;
    status:     uint8;
}

table MyRecord {
    timestamp:  Time;
    sensor:     SensorState;
    raw_bytes:  [uint8];
}

root_type MyRecord;
```

### Step 3: 编译 Schema

```bash
cd config/flatbuffers
flatc --cpp --bfbs-comments --bfbs-builtins \
  -o ../autogenerated_flatbuffers/ \
  MyRecord.fbs

# 产物:
# config/autogenerated_flatbuffers/MyRecord_generated.h
# config/autogenerated_flatbuffers/MyRecord.bfbs
```

### Step 4: CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(my-recorder)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FlatBuffers REQUIRED)
# find_package(OpenCV REQUIRED)  # 如不需要图像处理可省略

add_executable(my-recorder
    src/main.cpp
)

target_include_directories(my-recorder PRIVATE
    inc/
    config/autogenerated_flatbuffers/
)

target_link_libraries(my-recorder PRIVATE
    flatbuffers::flatbuffers
    /usr/lib/x86_64-linux-gnu/liblz4.so
    /usr/lib/x86_64-linux-gnu/libzstd.so
    # ${OpenCV_LIBS}  # 如不需要可省略
)
```

### Step 5: 主程序实现

`src/main.cpp`:

```cpp
#define MCAP_IMPLEMENTATION

#include "mcap/writer.hpp"
#include <flatbuffers/flatbuffers.h>
#include "MyRecord_generated.h"

#include <fstream>
#include <chrono>
#include <cstring>
#include <iostream>

// ═══ 工具函数 ═══

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::string result(f.tellg(), '\0');
    f.seekg(0);
    f.read(result.data(), result.size());
    return result;
}

uint64_t nanosecondsSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string getBagPath() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::localtime(&t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y_%m_%d_%H_%M_%S", tm);
    return "../bag/" + std::string(buf) + ".mcap";
}

// ═══ 录制类 ═══

class Recorder {
public:
    Recorder(const std::string& topic,
             const std::string& schema_name,
             const std::string& bfbs_path)
        : topic_(topic), schema_name_(schema_name),
          bfbs_path_(bfbs_path) {}

    void init(const std::string& output_path) {
        // 确保目录存在
        std::filesystem::path p(output_path);
        std::filesystem::create_directories(p.parent_path());

        // 打开 MCAP 文件
        auto status = writer_.open(output_path,
                                   mcap::McapWriterOptions("flatbuffer"));
        if (!status.ok()) {
            throw std::runtime_error("Open failed: " + status.message);
        }

        // 注册 Schema
        schema_ = std::make_unique<mcap::Schema>(
            schema_name_, "flatbuffer", readFile(bfbs_path_));
        writer_.addSchema(*schema_);

        // 注册 Channel
        channel_ = std::make_unique<mcap::Channel>(
            topic_, "flatbuffer", schema_->id);
        writer_.addChannel(*channel_);

        std::cout << "Recording: " << output_path
                  << " [schema_id=" << schema_->id
                  << ", channel_id=" << channel_->id << "]\n";
    }

    void writeFrame(uint32_t sensor_id, float value,
                    uint8_t status, const uint8_t* raw_data, size_t raw_size) {
        builder_.Clear();

        auto timestamp_ns = nanosecondsSinceEpoch();
        myapp::Time timestamp(timestamp_ns / 1'000'000'000,
                              timestamp_ns % 1'000'000'000);

        auto sensor = myapp::CreateSensorState(
            builder_, sensor_id, value, status);

        auto raw_vec = builder_.CreateVector(raw_data, raw_size);

        auto record = myapp::CreateMyRecord(
            builder_, &timestamp, sensor, raw_vec);

        builder_.Finish(record);

        mcap::Message msg;
        msg.channelId   = channel_->id;
        msg.sequence    = seq_++;
        msg.logTime     = timestamp_ns;
        msg.publishTime = timestamp_ns;
        msg.data        = reinterpret_cast<const std::byte*>(
                              builder_.GetBufferPointer());
        msg.dataSize    = builder_.GetSize();

        writer_.write(msg);
    }

    void close() {
        writer_.close();
        std::cout << "Recording finished.\n";
    }

private:
    mcap::McapWriter writer_;
    std::unique_ptr<mcap::Schema> schema_;
    std::unique_ptr<mcap::Channel> channel_;
    flatbuffers::FlatBufferBuilder builder_{1024};
    std::string topic_, schema_name_, bfbs_path_;
    uint32_t seq_ = 0;
};

// ═══ 主函数 ═══

int main() {
    try {
        Recorder rec("/my_topic",
                     "myapp.MyRecord",
                     "../config/autogenerated_flatbuffers/MyRecord.bfbs");

        rec.init(getBagPath());

        // 模拟写入 100 帧
        uint8_t dummy[16] = {};
        for (int i = 0; i < 100; i++) {
            rec.writeFrame(i, i * 1.5f, (i % 2) ? 1 : 0, dummy, sizeof(dummy));
            std::cout << "Frame " << i + 1 << " written\r" << std::flush;
        }

        rec.close();
        std::cout << "\nDone!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### Step 6: 编译运行

```bash
cd build
cmake ..
make -j$(nproc)
./my-recorder

# 输出:
# Recording: ../bag/2025_06_10_15_30_00.mcap [schema_id=1, channel_id=1]
# Frame 100 written
# Recording finished.
# Done!
```

---

## 六、最小可运行示例

下面是一个**不依赖 CMake**、单文件即可编译的最简 Demo：

```cpp
// minidemo.cpp —— 最简 MCAP + FlatBuffers 写入示例
// 编译: g++ -std=c++20 -I./inc -I/usr/local/include \
//         minidemo.cpp -o minidemo -lflatbuffers -llz4 -lzstd

#define MCAP_IMPLEMENTATION
#include "mcap/writer.hpp"
#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <chrono>
#include <cstring>
#include <iostream>

// 读取 BFBS 文件
std::string loadBfbs(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// 纳秒时间戳
uint64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int main() {
    // ═══ 1. 构建简易 FlatBuffer (手写，无需 flatc) ═══
    flatbuffers::FlatBufferBuilder builder(256);
    // 直接用底层 API 构建: [int32=42, float32=3.14, string="hello"]
    builder.Finish(builder.CreateString("hello mcap!"));
    // 注意: 这只是演示. 实际项目必须用 flatc 生成的 API 构建.

    // ═══ 2. 写入 MCAP ═══
    mcap::McapWriter writer;

    // 2a. 打开文件
    auto st = writer.open("demo.mcap", mcap::McapWriterOptions("flatbuffer"));
    if (!st.ok()) { std::cerr << st.message << "\n"; return 1; }

    // 2b. 注册 Schema (需要真实的 BFBS 文件)
    mcap::Schema schema("example.Message", "flatbuffer",
                        loadBfbs("your_schema.bfbs"));
    writer.addSchema(schema);

    // 2c. 注册 Channel
    mcap::Channel channel("/demo_topic", "flatbuffer", schema.id);
    writer.addChannel(channel);

    // 2d. 写入消息
    mcap::Message msg;
    msg.channelId   = channel.id;
    msg.sequence    = 1;
    msg.logTime     = nowNs();
    msg.publishTime = msg.logTime;
    msg.data        = (const std::byte*)builder.GetBufferPointer();
    msg.dataSize    = builder.GetSize();
    writer.write(msg);

    // 2e. 关闭
    writer.close();
    std::cout << "demo.mcap created!\n";
    return 0;
}
```

---

## 七、调试与验证

### 7.1 使用 MCAP CLI 工具

```bash
# 安装 MCAP CLI
npm install -g @mcap/cli

# 查看文件信息
mcap info output.mcap

# 查看 Chunk 结构
mcap info --chunks output.mcap

# 查看 Schema 列表
mcap info --schemas output.mcap

# 查看 Channel 列表
mcap info --channels output.mcap

# 提取消息到 JSON (通过 stdin/stdout pipe)
mcap cat output.mcap --json | head -50
```

### 7.2 使用 Foxglove Studio (GUI 可视化)

1. 下载 Foxglove Studio: https://foxglove.dev/download
2. 打开 `.mcap` 文件
3. 自动识别 BFBS Schema，解析所有字段
4. 可播放、拖动时间轴、查看图像/曲线/原始数据

### 7.3 Python 读取验证

```python
from mcap.reader import make_reader
from mcap_flatbuffers.decoder import FlatbuffersDecoder

with open("output.mcap", "rb") as f:
    reader = make_reader(f, decoder_factories=[FlatbuffersDecoder()])
    for schema_, channel, message in reader.iter_messages():
        print(f"Topic={channel.topic} Time={message.log_time} Size={message.data}")
```

### 7.4 常见问题排查

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| `Failed to open MCAP file` | 目录不存在 | 确保调用 `create_directories()` |
| Foxglove 无法解析数据 | BFBS 与数据不匹配 | 使用同一次 `flatc` 编译的产物 |
| 文件损坏/无法打开 | 程序崩溃未调用 `close()` | 需调用 `close()` 写入 Footer |
| 编译错误 `undefined reference to flatbuffers` | 未链接 FlatBuffers 库 | 添加 `-lflatbuffers` |
| `MCAP_PUBLIC` 未定义 | 未引入 visibility.hpp | 确认 `#include "mcap/visibility.hpp"` |

---

## 八、附录：依赖版本清单

| 组件 | 版本 | 安装方式 | 备注 |
|------|------|----------|------|
| Ubuntu | 22.04 LTS | — | 操作系统 |
| GCC | 11.4.0 | `apt install build-essential` | 需要 C++20 |
| CMake | 3.22+ | `apt install cmake` | |
| FlatBuffers | 25.2.10 | 源码编译 | flatc + libflatbuffers.a |
| MCAP | 1.3.0 | header-only | 嵌入在 `src/utility/inc/mcap/` |
| LZ4 | 1.9.3 | `apt install liblz4-dev` | MCAP 压缩依赖 |
| Zstd | 1.4.8 | `apt install libzstd-dev` | MCAP 压缩依赖 |
| OpenCV | 4.5.4 | `apt install libopencv-dev` | 图像处理（可选） |

### 快速环境搭建脚本

```bash
#!/bin/bash
# setup_env.sh —— 一键安装所有依赖

set -e

echo "=== 安装系统包 ==="
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
                    liblz4-dev libzstd-dev libopencv-dev

echo "=== 编译安装 FlatBuffers ==="
cd /tmp
git clone --depth 1 --branch v25.2.10 \
    https://github.com/google/flatbuffers.git flatbuffers-src
cd flatbuffers-src
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DFLATBUFFERS_BUILD_TESTS=OFF \
    -DFLATBUFFERS_BUILD_FLATHASH=OFF \
    -DFLATBUFFERS_BUILD_FLATLIB=OFF
make -j$(nproc)
sudo make install
sudo ldconfig

echo "=== 验证 ==="
flatc --version
pkg-config --modversion flatbuffers
echo "=== 环境配置完成! ==="
```

---

## 总结：从零实现 MCAP 录制的核心步骤

```
① 安装依赖         →  flatc, libflatbuffers, lz4, zstd
② 定义 .fbs Schema  →  描述你的数据结构
③ 编译 Schema       →  flatc → _generated.h + .bfbs
④ 集成 MCAP 头文件   →  将 mcap/ 头文件放入项目中
⑤ 实现 Recorder 类  →  init() → writeFrame() → close()
⑥ 编译运行          →  g++/cmake, 产出 .mcap 文件
⑦ 验证             →  Foxglove Studio 或 mcap CLI
```
