#pragma once
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sys/mman.h>
#include <thread>

#include <frame_data.hpp>
#include <signal.h>
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
private:
	//libcamera相关
	std::shared_ptr<libcamera::Camera> m_camera;
	std::unique_ptr<libcamera::CameraManager> m_camera_manager;	
	libcamera::FrameBufferAllocator *m_frame_buffer_allocator;
	libcamera::Stream *m_stream;
	void request_complete(libcamera::Request *request);
	std::vector<std::unique_ptr<libcamera::Request>> m_requests;
	
	//相机参数
	int m_width;
	int m_height;
	int m_crop_width;
	int m_crop_height;
	int m_stride;
	int m_exposure_time_us;
	int m_frame_duration_us;

	libcamera::Rectangle m_center_crop;

	//线程相关
	std::mutex m_mtx;
	std::condition_variable m_plane_condition_variable;
	
	//状态
	bool m_has_new_frame;
	bool m_stopped;

        bool get_crop();
	
public:
	FrameData m_latest_frame;
	cv::Mat m_rgb_frame;
	bool initialize();
	void stop();
	std::unordered_map<int, MappedPlane> m_mapped_planes;
	cv::Mat plane_to_rgb_mat(const FrameData& frame);
	cv::Mat wait_and_get_latest_frame();
};

