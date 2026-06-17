#ifndef DETECTOR_HPP
#define DETECTOR_HPP
#include <chrono>
#include <cstdint>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include <data_type.hpp>
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
    const VisionData& vision_data() const { return m_vision_data; }

private:
    // ── 检测结果 ──
    VisionData m_vision_data;

    // ── Bayer 差异阈值 ──
    int    m_diff_threshold      = config::DIFF_THRESHOLD;
    int    m_roi_width           = config::ROI_WIDTH;
    int    m_roi_height          = config::ROI_HEIGHT;
    double m_best_circularity_standard = config::BEST_CIRCULARITY_STANDARD;

    State m_state = State::LOST;

    cv::Rect m_roi_rect{0, 0, 0, 0};

    // ── 时间统计 ──
    std::chrono::steady_clock::time_point m_now{};
    std::chrono::steady_clock::time_point m_last{};

    int    m_index    = 0;

    // ── 辅助函数 ──
    void set_roi(const cv::Size& frame_size);
};

#endif // DETECTOR_HPP
