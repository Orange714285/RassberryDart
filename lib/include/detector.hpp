#pragma once

#include <atomic>
#include <condition_variable> 
#include <cmath>
#include <chrono>
#include <deque>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <signal.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include "frame_data.hpp"
#include "recorder.hpp"

#include <Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <libcamera/libcamera.h>

class Detector
{
public:
	Detector();
	~Detector();
	
	std::ofstream m_outfile;
	void detect_and_draw_lights(cv::Mat &frame);

	int m_rmin = 0;
   	int m_gmin = 160;
    	int m_bmin = 0;
    	int m_rmax = 140;
    	int m_gmax = 255;
    	int m_bmax = 140;

	cv::Mat plane_to_rgb_mat(const FrameData& frame);
private:

	int m_index = 0;

	double contourCircularity(const std::vector<cv::Point>& contour);
	
    	class DetectResult
   	{
  	public:
        	int index = 0;
        	float pixel_x = 0;
        	float pixel_y = 0;
        	int8_t frame_dtMs = 0;
   	};

    	DetectResult m_detect_result;
    	std::chrono::steady_clock::time_point m_now{};
    	std::chrono::steady_clock::time_point m_last{};

	libcamera::FrameBuffer::Plane m_plane;
	//std::unordered_map<int, MappedPlane> m_mapped_planes;
	cv::Mat m_frame;
};
