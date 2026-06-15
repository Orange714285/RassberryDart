# 树莓派本地开发\(ubuntu版本\)

## 前言 

树莓派本地开发可以提升工作效率，具体而言：我们在树莓派编辑代码，然后编译代码，得到可执行文件，再将可执行文件传入树莓派并且在树莓派进行验证；而这一过程有些复杂；本文档意在梳理完整流程，方便后续开发环境配置

（靠我真要快不会用VSCode了）

美中不足的是我现在还用不了debug，因为debug目前只能在本地跑





## 1\. 网络搭建

给树莓派烧录系统的时候，我们就已经设置好了树莓派应该连接的热点的名称及其密码；而在windows下，我们需要通过windows开放热点（其热点名和密码与树莓派匹配），这个时候打开windows热点，树莓派的ip就会被识别到了；然后我们就可以在cmd 通过ssh命令连接树莓派；也就是说，windows系统下的树莓派开发，需要windows为树莓派提供热点

1. 树莓派热点连接ubuntu

ubuntu 需要打开热点，并且热点名称及其密码与树莓派匹配；然后树莓派就会自动连接该热点；

要验证此事，我们在ubuntu终端执行：

```Bash
sudo arp-scan -l
```

样例输出为：

```Bash
orange@orange-Lenovo-ThinkBook-15p-Gen-2:~/pi-workspace/spi_test/build$ sudo arp-scan -l
[sudo] orange 的密码： 
Interface: wlp0s20f3, type: EN10MB, MAC: f8:9e:94:f9:d4:22, IPv4: 10.42.0.1
Starting arp-scan 1.9.7 with 256 hosts (https://github.com/royhills/arp-scan)
10.42.0.219     f0:40:af:90:12:50       (Unknown)

1 packets received by filter, 0 packets dropped by kernel
Ending arp-scan 1.9.7: 256 hosts scanned in 1.895 seconds (135.09 hosts/sec). 1 responded
orange@orange-Lenovo-ThinkBook-15p-Gen-2:~/pi-workspace/spi_test/build$ 
```

这里树莓派ID是：10\.42\.0\.219

连接树莓派：

```Bash
orange@orange-Lenovo-ThinkBook-15p-Gen-2:~/pi-workspace/spi_test/build$ ssh rp@10.42.0.219
rp@10.42.0.219's password: 
Linux rp 6.12.47+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.12.47-1+rpt1 (2025-09-16) aarch64

The programs included with the Debian GNU/Linux system are free software;
the exact distribution terms for each program are described in the
individual files in /usr/share/doc/*/copyright.

Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
permitted by applicable law.
Last login: Tue Apr 28 21:35:42 2026 from 10.42.0.1
```



2. 手机有线网络供应

然而受限于ubuntu的网卡冲突，ubuntu 无法在连接无线网的同时打开热点，于是我们需要用一根USB\-typeC的线，来连接手机和电脑，从而给电脑供应有线网络。然后打开手机的“USB共享网络”（需要先接线再共享网络，否则手机可能找不到USB共享网络）







## 2\.树莓派交叉编译

首先了解树莓派系统，在树莓派终端输入



得到：树莓派是64 位 aarch64 系统，我们需要正确安装相应的交叉编译的工具

```Bash
sudo apt update
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

正确安装交叉编译工具后，为了验证交叉编译工具能够正常工作，我们在ubuntu本地编译test\.c 文件：

```Bash
aarch64-linux-gnu-gcc test.c -o test
```

> 什么你问test\.c 是什么？
> 
> 当然是Hello , World \!
> 
> 

然后在ubuntu本地，把test上传到树莓派home目录并且远程执行 ：

```Bash
# 上传文件到树莓派家目录
scp test rp@10.42.0.219:~

# 远程执行
ssh rp@10.42.0.219 ./test
```

得到结果：



但如果我们不使用交叉编译工具来编译make，或者使用了错误的交叉编译工具（例如32位的ARM编辑器）进行编译呢？

```Bash
arm-linux-gnueabihf-gcc test.c -o test
```

那么上传树莓派后

```Bash
# 上传文件到树莓派家目录
scp test rp@10.42.0.219:~

# 远程执行
ssh rp@10.42.0.219 ./test
```

就会出现报错


```Bash
./test: cannot execute: required file not found    
```

树莓派是64 位 aarch64 系统，32 位架构不兼容。



**为什么我们需要交叉编译？**

Ubuntu 电脑：Intel/AMD x86\_64 架构，CPU 只认识 x86\_64 指令集

树莓派：ARM64（aarch64）架构，CPU 只认识 ARM64 指令集

我们在ubuntu电脑下，编译器输出 x86\_64 机器码，拷贝到树莓派上，树莓派是无法执行的

我们需要在x86\_64的ubuntu电脑上编译出ARM64架构下CPU能识别的机器码



3\. 同步树莓派头文件与库

在 home 文件夹下创建sysroot目录，用于同步树莓派上的头文件与库

```Bash
mkdir -p ~/rpi-sysroot
```

用正确路径同步树莓派

```Bash
rsync -avz rp@10.42.0.219:/usr ~/rpi-sysroot/
    rsync -avz rp@10.42.0.219:/lib ~/rpi-sysroot/
    rsync -avz rp@10.42.0.219:/etc ~/rpi-sysroot/
```

## 3\.  配置cmake

cmake 本身并不知道自己要给树莓派编译；因此，我们也需要对cmake 也进行相关设置

在项目根目录创建toolchain\.cmake

```CMake
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
```

在此之后，我们进行cmake 就不能只是单纯的进行cmake \.\. ，我们需要将上述cmake 配置应用到cmake 上：

```Bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
```

不过这样一长串我们很难记住，于是

```Bash
echo "alias rpi-cmake='cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake'" >> ~/.bashrc && source ~/.bashrc
```

这样一来 `rpi-cmake `就等同于 `cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake`

## 4\. VSCode 智能感知

\#\#\#\#  方案一 ： 通过 \.json文件配置智能感知

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NDVhMGQ2MTY4NzVjMDI1ZDc4MTkxYzM1NTU2YjRhYzFfYThkOWM2ZWM4MGE3ZDY0ZjViMjVlOTRmYjQ1MjRkY2FfSUQ6NzYzNDQ3OTcyNzU4MzQ0ODI1OV8xNzgxMDE2NjcxOjE3ODExMDMwNzFfVjM)

对于当前项目代码，在根目录下新建文件夹 \.vscode文件夹，并且创建并且编辑json文件

```JSON
{
    "configurations": [
        {
            "name": "Raspberry Pi ARM64", // 配置名称：树莓派64位
            "compilerPath": "/usr/bin/aarch64-linux-gnu-gcc", // 配置交叉编译器
            "intelliSenseMode": "linux-gcc-arm64", // 智能感知模式：ARM64 Linux
            "cStandard": "c17",
            "cppStandard": "c++17",
            "includePath": [ // 头文件搜索路径（应根据项目实际情况而修改）
                "${workspaceFolder}/**",
                "${workspaceFolder}/lib/include",
                "${HOME}/rpi-sysroot/usr/include",
                "${HOME}/rpi-sysroot/usr/include/aarch64-linux-gnu"
            ],
            "defines": [ //d efines 宏定义
                "__aarch64__",
                "__linux__"
            ]
        }
    ],
    "version": 4
}
```

重启VSCode 即可生效



方案二: cmake tool 配置

配置cmake tool 也需要写如下\.json文件，在项目根目录的\.vscode 文件夹下创建setting\.json文件

```JSON
{
    // 自动使用交叉编译工具链
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "${workspaceFolder}/toolchain.cmake"
    },
    // Build 目录
    "cmake.buildDirectory": "${workspaceFolder}/build",
    // 你的编译器（ARM64）
    "cmake.generator": "Unix Makefiles",
    // C++ 版本
    "cppStandard": "17",
    // 自动关联 C/C++ 插件（智能感知自动同步）
    "cmake.configureOnOpen": true
}
```



将编译好的可执行文件上传到树莓派上并运行



