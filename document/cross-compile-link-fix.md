# Ubuntu 22.04 交叉编译 Raspberry Pi (Debian 13) 项目链接错误修复指南

## 问题现象

在 Ubuntu 22.04 主机上使用 `aarch64-linux-gnu-gcc/g++` 交叉编译树莓派项目时，`make` 在链接阶段报大量 `undefined reference` 错误：

```
undefined reference to `std::ios_base_library_init()@GLIBCXX_3.4.32'
undefined reference to `fmod@GLIBC_2.38'
undefined reference to `__isoc23_strtol@GLIBC_2.38'
undefined reference to `arc4random_buf@GLIBC_2.36'
undefined reference to `__cxa_call_terminate@CXXABI_1.3.15'
...
```

同时还有：
```
warning: liblapack.so.3, needed by libopencv_core.so, not found
warning: libblas.so.3, needed by libopencv_core.so, not found
```

---

## 根因分析

### 1. glibc / libstdc++ 版本不匹配（主要原因）

| 组件 | 树莓派 sysroot | 主机 (Ubuntu 22.04) | 兼容？ |
|------|---------------|---------------------|--------|
| glibc | **2.41** (Debian 13) | **2.35** | ❌ |
| libstdc++ | **6.0.33** (gcc 14) | gcc 11.4 提供 | ❌ |
| gcc | 14 | 11.4 | ❌ |

树莓派运行的是 Debian 13 (Trixie)，预编译的 OpenCV、libcamera 等库是针对 glibc 2.41 + gcc 14 编译的。

主机 Ubuntu 22.04 的交叉编译器是 gcc 11.4，链接时默认使用自己的 libstdc++（版本过低），导致 sysroot 库引用的新版本符号无法解析：

- `@GLIBCXX_3.4.32` → 需要 libstdc++ from gcc ≥ 14
- `@CXXABI_1.3.15` → 需要 libstdc++ from gcc ≥ 14  
- `@GLIBC_2.36` → 需要 glibc ≥ 2.36
- `@GLIBC_2.38` → 需要 glibc ≥ 2.38

**快速自查方法**：

```bash
# 查看 sysroot 中 glibc 版本
cat ~/rpi-sysroot/lib/aarch64-linux-gnu/libc.so.6 | strings | grep "GLIBC "

# 查看主机交叉编译器 glibc 版本
ldd --version | head -1

# 查看 sysroot 中 opencv 需要的符号版本
aarch64-linux-gnu-readelf -V ~/rpi-sysroot/usr/lib/aarch64-linux-gnu/libopencv_core.so 2>/dev/null | grep GLIBC
```

### 2. binutils 2.38 的 sysroot 绝对符号链接 bug（次要原因）

sysroot 中 `liblapack.so.3` 和 `libblas.so.3` 使用了绝对路径符号链接：

```
libblas.so.3 → /etc/alternatives/libblas.so.3-aarch64-linux-gnu
                 → /usr/lib/aarch64-linux-gnu/blas/libblas.so.3
```

Ubuntu 22.04 的 binutils 2.38 在解析 sysroot 内 NEEDED 库的绝对符号链接时，不会正确应用 `--sysroot` 前缀，而是直接在宿主机文件系统中查找，导致找不到文件。

> 此 bug 在 binutils 2.39+ 修复，参见 [sourceware#29190](https://sourceware.org/bugzilla/show_bug.cgi?id=29190)。

---

## 修复方案

> **不需要下载新版 ARM 工具链**，只需两步即可解决。

### 步骤 1：修改 `toolchain.cmake`

强制链接器优先使用 sysroot 自带的 glibc 和 libstdc++（而非主机 gcc 11 的旧版本）：

```cmake
# toolchain.cmake 末尾添加

# 强制链接器优先使用 sysroot 中的 libc/libstdc++（而非主机交叉编译器的旧版本）
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
    -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -L${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14 \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")

SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} \
    -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -L${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14 \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")
```

**说明**：
- `-L<path>` — 将 sysroot 的库搜索路径放在链接器搜索顺序的最前面
- `-Wl,-rpath-link,<path>` — 让链接器在解析 NEEDED 依赖时也优先搜索 sysroot

### 步骤 2：修复 sysroot 中的绝对符号链接

```bash
cd ~/rpi-sysroot/usr/lib/aarch64-linux-gnu

# 将绝对符号链接改为相对符号链接
ln -sf blas/libblas.so.3.12.1 libblas.so.3
ln -sf lapack/liblapack.so.3.12.1 liblapack.so.3
```

> **注意**：每次从树莓派重新 `rsync` sysroot 后，这两个符号链接会被覆盖，需要重新执行。建议将这两行命令加入 sysroot 同步脚本。

---

## 完整 `toolchain.cmake` 参考

```cmake
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR aarch64)

SET(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

SET(CMAKE_SYSROOT $ENV{HOME}/rpi-sysroot)

SET(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config 配置
SET(ENV{PKG_CONFIG_DIR} "")
SET(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
SET(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
SET(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config" CACHE FILEPATH "pkg-config executable")

# 强制链接器优先使用 sysroot 中的 libc/libstdc++（而非主机交叉编译器的旧版本）
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
    -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -L${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14 \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")

SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} \
    -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -L${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14 \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu \
    -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")
```

---

## 构建命令

```bash
cd ~/pi-workspace/LightDetect
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
make -j$(nproc)
```

---

## 补充：动态库 vs 静态库的选择

如果项目使用**动态库 (SHARED)**：
- 优点：多程序共享，节省磁盘空间
- 缺点：部署时需要将 `.so` 复制到树莓派，并设置 `LD_LIBRARY_PATH` 或 `RPATH=$ORIGIN`

如果使用**静态库 (STATIC)**：
- 优点：可执行文件自包含，直接 `scp` 就能运行，无依赖
- 缺点：可执行文件体积较大

本项目已将核心库 `libdart` 改为静态库，简化部署流程。

---

## 故障排查速查表

| 错误信息关键字 | 可能原因 | 检查方法 |
|---------------|---------|---------|
| `@GLIBC_2.xx` 未定义 | glibc 版本不匹配 | `cat ~/rpi-sysroot/lib/*/libc.so.6 \| strings \| grep GLIBC` |
| `@GLIBCXX_3.4.xx` 未定义 | libstdc++ 版本不匹配 | 检查 sysroot 中 gcc 版本号路径 |
| `liblapack.so.3` not found | 绝对符号链接 bug | `ls -la ~/rpi-sysroot/usr/lib/*/liblapack.so.3` |
| 运行时找不到 `.so` | 动态库未部署 / RPATH 错误 | `readelf -d ./program \| grep RPATH` |
