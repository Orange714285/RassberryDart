#ifndef DETECTOR_HPP
#define DETECTOR_HPP

#include <opencv2/opencv.hpp>
#include <chrono>
#include <fstream>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <utility>
#include <Eigen/Dense>  // 包含 Eigen 库头文件  
enum class State
{
    LOST,
    DROP,
    FOUND
};
class Detector
{
public:
    Detector();
    ~Detector();
    void detect_and_draw_lights(cv::Mat &frame);
    bool is_contour_touch_border(const std::vector<cv::Point>& contour, 
                          int img_width, 
                          int img_height
                          );
private:
    int m_max_val;
    int m_kernel_close_size;
    int m_kernel_open_size;
    int m_roi_width;
    int m_roi_height;

    std::ofstream m_outfile;
    int m_index = 0;
    size_t m_sum_dtMs = 0;

    double contourCircularity(const std::vector<cv::Point>& contour);
    void set_roi(const cv::Size& frame_size, bool is_set_roi);

    class DetectResult
    {
    public:
        int index = 0;
        float pixel_x = 0;
        float pixel_y = 0;
        int8_t frame_dtMs = 0;
    };
    DetectResult m_detect_result;
    State m_state = State::LOST;
    
    cv::Rect m_roi_rect{0, 0, 0, 0};  // x, y, w, h

    std::chrono::steady_clock::time_point m_now{};
    std::chrono::steady_clock::time_point m_last{};
};

#endif // DETECTOR_HPP

