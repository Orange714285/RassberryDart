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

# 添加以下内容解决 pkg-config 问题
SET(ENV{PKG_CONFIG_DIR} "")
SET(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
SET(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

# 设置 CMake 的 pkg-config 使用 sysroot
SET(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config" CACHE FILEPATH "pkg-config executable")

# 强制链接器优先使用 sysroot 中的 libc/libstdc++（而非主机交叉编译器的旧版本）
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -L${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14 -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -L${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14 -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")
