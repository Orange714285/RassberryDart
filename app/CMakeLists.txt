set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME DartVision
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${LIBCAMERA_INCLUDE_DIRS}
    ${OPENCV_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${LIBCAMERA_LIBRARY_DIRS}
    ${OPENCV_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    dart
    ${LIBCAMERA_LIBRARIES}
    ${OPENCV_LIBRARIES}
)


