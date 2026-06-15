cmake_minimum_required(VERSION 3.10)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(OPENCV REQUIRED opencv4)

find_package(Eigen3 REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_library(dart STATIC
	${SRC_LIST}
)

target_include_directories(dart PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/config
	${LIBCAMERA_INCLUDE_DIRS}
	${OPENCV_INCLUDE_DIRS}
)

target_link_directories(dart PRIVATE
	${LIBCAMERA_LIBRARY_DIRS}
	${OPENCV_LIBRARY_DIRS}
)

target_link_libraries(dart PUBLIC
	pigpio 
	gpiod
	Eigen3::Eigen
	flatbuffers::flatbuffers
	zstd
	lz4
	${LIBCAMERA_LIBRARIES}
	${OPENCV_LIBRARIES}
)

target_compile_options(dart PRIVATE
	${LIBCAMERA_CFLAGS_OTHER}
	${OPENCV_CFLAGS_OTHER}
)


		

