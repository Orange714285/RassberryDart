#include "detector.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>

Detector::Detector()
{
    m_state = State::LOST;
    m_diff_gray = cv::Mat(config::CAM_HEIGHT / 2, config::CAM_WIDTH / 2, CV_8UC1);
    m_binary    = cv::Mat(config::CAM_HEIGHT / 2, config::CAM_WIDTH / 2, CV_8UC1);
}

Detector::~Detector()
{
}

void Detector::detect_and_draw_lights(cv::Mat &bayer_frame)
{
    m_now = std::chrono::steady_clock::now();
    m_detect_result.frame_dtMs = static_cast<uint16_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(m_now - m_last).count());
    m_last = m_now;

    int h = bayer_frame.rows;
    int w = bayer_frame.cols;

    // 1. 对每个 2x2 RGGB 块计算 (G0+G1)/2 - (R+B)/2
    //    diff_u8 = clamp(((G0+G1)-(R+B))/4 + 128, 0, 255)
    for (int row = 0; row < h; row += 2)
    {
        const uint8_t* src0 = bayer_frame.ptr<const uint8_t>(row);
        const uint8_t* src1 = bayer_frame.ptr<const uint8_t>(row + 1);
        uint8_t* dst = m_diff_gray.ptr<uint8_t>(row / 2);

        for (int col = 0; col < w; col += 2)
        {
            int g_sum = src0[col + 1] + src1[col];
            int rb_sum = src0[col] + src1[col + 1] + 1;

            int ratio_x256 = (g_sum << 8) / (rb_sum*2);
            int diff = ratio_x256-128   ;
            dst[col / 2] = static_cast<uint8_t>(std::clamp(diff, 0, 255));
        }
    }

    // 2. 写回原图: diff 赋值给 G 位置, R/B 位置写黑
    for (int row = 0; row < h / 2; row++)
    {
        const uint8_t* src = m_diff_gray.ptr<const uint8_t>(row);
        uint8_t* dst0 = bayer_frame.ptr<uint8_t>(row * 2);
        uint8_t* dst1 = bayer_frame.ptr<uint8_t>(row * 2 + 1);

        for (int col = 0; col < w / 2; col++)
        {
            uint8_t val = src[col];
            dst0[col * 2]     = 0;    // R → black
            dst0[col * 2 + 1] = val;  // G₀ → diff
            dst1[col * 2]     = val;  // G₁ → diff
            dst1[col * 2 + 1] = 0;    // B → black
        }
    }

        set_roi(bayer_frame.size());

        // 直接从 ROI 区域做单通道阈值二值化（应用实时阈值参数）
        cv::Mat frame_roi = bayer_frame(m_roi_rect);
        cv::Mat binary;
        cv::threshold(frame_roi, binary, 120, 255, cv::THRESH_BINARY);

        // 查找轮廓（复用 m_contours 成员避免每帧堆分配）
        m_contours.clear();
        cv::findContours(binary, m_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);


        double best_area  = 0.0;
        int    best_index = -1;

        for (int i = 0; i < static_cast<int>(m_contours.size()); i++)
        {
            cv::Rect bbox = cv::boundingRect(m_contours[i]);

            if (is_contour_touch_border(m_contours[i], binary.cols, binary.rows))
                continue;

            double area = cv::contourArea(m_contours[i]);
            if (area >= best_area)
            {
                best_index = i;
                best_area  = area;
            }
        }

        if (best_index >= 0)
        {
            cv::Rect best_bbox = cv::boundingRect(m_contours[best_index]);
            cv::Rect best_bbox_on_frame(
                best_bbox.x + m_roi_rect.x, best_bbox.y + m_roi_rect.y,
                best_bbox.width, best_bbox.height);
            m_detect_result.pixel_x = best_bbox_on_frame.x + best_bbox_on_frame.width  / 2.0f;
            m_detect_result.pixel_y = best_bbox_on_frame.y + best_bbox_on_frame.height / 2.0f;
            cv::rectangle(bayer_frame, best_bbox_on_frame, cv::Scalar(255, 255, 0), 1);
            m_state = State::FOUND;
        }
        else
        {
            m_detect_result.pixel_x = 0;
            m_detect_result.pixel_y = 0;
            m_state = State::LOST;
        }

        m_index++;
        m_detect_result.index = m_index;

        cv::rectangle(bayer_frame, m_roi_rect, cv::Scalar(255, 255, 255), 1);

}

double Detector::contourCircularity(const std::vector<cv::Point>& contour)
{
    const double area = cv::contourArea(contour);
    if (area <= 0.0) return 0.0;

    const double perimeter = cv::arcLength(contour, true);
    if (perimeter <= 0.0) return 0.0;

    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

bool Detector::is_contour_touch_border(const std::vector<cv::Point>& contour,
                                        int img_width,
                                        int img_height)
{
    const int margin = 3;
    cv::Rect rect = cv::boundingRect(contour);

    bool touch_left   = (rect.x <= margin);
    bool touch_right  = (rect.x + rect.width >= img_width - margin);
    bool touch_top    = (rect.y <= margin);
    bool touch_bottom = (rect.y + rect.height >= img_height - margin);

    return touch_left || touch_right || touch_top || touch_bottom;
}

    void Detector::set_roi(const cv::Size& frame_size)
    {
        if (frame_size.width <= 0 || frame_size.height <= 0)
        {
            m_roi_rect = cv::Rect();
            return;
        }

        const cv::Rect frame_rect(0, 0, frame_size.width, frame_size.height);

        if (m_state == State::LOST)
        {
            m_roi_rect = frame_rect;
        }
        else if (m_state == State::FOUND)
        {
            const int roi_x = static_cast<int>(std::round(m_detect_result.pixel_x - m_roi_width  / 2.0));
            const int roi_y = static_cast<int>(std::round(m_detect_result.pixel_y - m_roi_height / 2.0));
            m_roi_rect = cv::Rect(roi_x, roi_y, m_roi_width, m_roi_height) & frame_rect;

            if (m_roi_rect.width <= 0 || m_roi_rect.height <= 0)
                m_roi_rect = frame_rect;
        }
        else
        {
            m_roi_rect = frame_rect;
        }
    }