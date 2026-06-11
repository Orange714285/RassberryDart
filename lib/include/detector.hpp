#ifndef DETECTOR_HPP
#define DETECTOR_HPP
#include <chrono>
#include <cstdint>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include <config.hpp>

enum class State
{
    LOST,
    FOUND
};

class Detector
{
public:
    Detector();
    ~Detector();

    void detect_and_draw_lights(cv::Mat &frame);
    void set_hsv_params(int h_low, int h_high, int s_low, int s_high, int v_low, int v_high);

private:
    // ── HSV 阈值 (OpenCV: H=0..180, S/V=0..255) ──
    int m_h_low  = config::HSV_H_LOW,   m_h_high = config::HSV_H_HIGH;
    int m_s_low  = config::HSV_S_LOW,   m_s_high = config::HSV_S_HIGH;
    int m_v_low  = config::HSV_V_LOW,   m_v_high = config::HSV_V_HIGH;
    int m_roi_width                   = config::ROI_WIDTH;
    int m_roi_height                  = config::ROI_HEIGHT;
    double m_best_circularity_standard = config::BEST_CIRCULARITY_STANDARD;

    // ── 检测结果 ──
    class DetectResult
    {
    public:
        int     index      = 0;
        float   pixel_x    = 0;
        float   pixel_y    = 0;
        uint16_t frame_dtMs = 0;
    };
    DetectResult m_detect_result;
    State m_state = State::LOST;

    cv::Rect m_roi_rect{0, 0, 0, 0};

    // ── 时间统计 ──
    std::chrono::steady_clock::time_point m_now{};
    std::chrono::steady_clock::time_point m_last{};

    int     m_index    = 0;
    size_t  m_sum_dtMs = 0;

    // ── 可复用的工作缓冲区 ──
    std::vector<std::vector<cv::Point>> m_contours;

    // ── 辅助函数 ──
    double contourCircularity(const std::vector<cv::Point>& contour);
    bool   is_contour_touch_border(const std::vector<cv::Point>& contour,
                                   int img_width, int img_height);
    void   set_roi(const cv::Size& frame_size);
};

#endif // DETECTOR_HPP
