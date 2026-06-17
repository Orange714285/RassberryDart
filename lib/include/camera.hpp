#pragma once
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sys/mman.h>
#include <unordered_map>
#include <vector>

#include <data_type.hpp>
#include <config.hpp>
#include <opencv2/opencv.hpp>
#include <libcamera/libcamera.h>

struct MappedPlane
{
    void* addr = nullptr;
    size_t length = 0;
};

class Camera
{
public:
    Camera();
    ~Camera();

    bool start();
    void stop();

    cv::Mat wait_and_get_latest_frame();

    // getters
    int width()  const { return m_width; }
    int height() const { return m_height; }
    int fps()    const { return m_fps; }

private:
    // ── libcamera 相关 ──
    std::shared_ptr<libcamera::Camera>          m_camera;
    std::unique_ptr<libcamera::CameraManager>   m_camera_manager;
    std::unique_ptr<libcamera::FrameBufferAllocator> m_frame_buffer_allocator;
    libcamera::Stream*                          m_stream = nullptr;
    std::vector<std::unique_ptr<libcamera::Request>> m_requests;

    void request_complete(libcamera::Request *request);

    // ── 相机参数 ──
    int   m_width               = config::CAM_WIDTH;
    int   m_height              = config::CAM_HEIGHT;
    int   m_crop_width          = config::CAM_CROP_WIDTH;
    int   m_crop_height         = config::CAM_CROP_HEIGHT;
    int   m_crop_x              = config::CAM_CROP_X;
    int   m_crop_y              = config::CAM_CROP_Y;
    int   m_stride              = 0;
    int   m_exposure_time_us    = config::CAM_EXPOSURE_TIME_US;
    float m_brightness          = config::CAM_BRIGHTNESS;
    int   m_fps                 = config::CAM_FPS;
    int   m_colour_temperature  = config::CAM_COLOUR_TEMP;
    libcamera::Rectangle m_center_crop;

    bool get_crop();

    // ── 线程同步 ──
    int                    m_index = 0;
    std::mutex             m_mtx;
    std::condition_variable m_plane_condition_variable;

    // ── 状态 ──
    bool m_stopped = false;

    // ── 帧缓冲（单槽覆盖，始终取最新帧，保证强实时性） ──
    cv::Mat   m_frame_slot;
    bool      m_frame_ready = false;
    std::unordered_map<int, MappedPlane>       m_mapped_planes;

    FrameData m_latest_frame;
    cv::Mat   m_rgb_frame;

    cv::Mat plane_to_rgb_mat(const FrameData& frame);
};
