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

    void detect_and_draw_lights(cv::Mat &bayer_frame);

    const cv::Mat& diff_gray() const { return m_diff_gray; }
    const cv::Mat& binary()    const { return m_binary; }

private:
    // ── Bayer 差异阈值 ──
    int    m_diff_threshold      = 200; // 绿色: diff_u8 > 200 (ratio ≈ 2.1)
    int    m_roi_width           = config::ROI_WIDTH;
    int    m_roi_height          = config::ROI_HEIGHT;
    double m_best_circularity_standard = config::BEST_CIRCULARITY_STANDARD;

    // ── 输出图像 ──
    cv::Mat m_diff_gray;  // 320x240 CV_8UC1, 差异灰度图
    cv::Mat m_binary;     // 320x240 CV_8UC1, 二值化结果

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
    void set_roi(const cv::Size& frame_size);

};

#endif // DETECTOR_HPP
