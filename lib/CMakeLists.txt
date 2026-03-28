cmake_minimum_required(VERSION 3.10)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(OPENCV REQUIRED opencv4)  # 或 opencv
pkg_check_modules(EIGEN3 REQUIRED eigen3)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(libdart SHARED 
	${SRC_LIST}
)

target_include_directories(libdart PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBCAMERA_INCLUDE_DIRS}
        ${OPENCV_INCLUDE_DIRS}
	${EIGEN3_INCLUDE_DIRS}
)

target_link_directories(libdart PRIVATE
        ${LIBCAMERA_LIBRARY_DIRS}
        ${OPENCV_LIBRARY_DIRS}
)

target_link_libraries(libdart PRIVATE
        ${LIBCAMERA_LIBRARIES}
        ${OPENCV_LIBRARIES}
)

target_compile_options(libdart PRIVATE
    ${LIBCAMERA_CFLAGS_OTHER}
    ${OPENCV_CFLAGS_OTHER}
    ${EIGEN3_CFLAGS_OTHER}
)

		

