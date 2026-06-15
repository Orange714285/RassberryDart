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

    // // 3. 二值化 → bayer_frame 现在是 640x480 二值图
    // cv::threshold(bayer_frame, bayer_frame, m_diff_threshold, 255, cv::THRESH_BINARY);

    // // 4. 在 640x480 二值图上查找轮廓
    // m_contours.clear();
    // cv::findContours(bayer_frame, m_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // double best_circularity = 0.0;
    // int    best_index       = -1;
    // for (size_t i = 0; i < m_contours.size(); i++)
    // {
    //     double cur_circularity = contourCircularity(m_contours[i]);

    //     if (is_contour_touch_border(m_contours[i], w, h))
    //         continue;
    //     if (cur_circularity >= best_circularity)
    //     {
    //         best_index = static_cast<int>(i);
    //         best_circularity = cur_circularity;
    //     }
    // }

    // if (best_circularity > m_best_circularity_standard && best_index >= 0)
    // {
    //     cv::Rect best_bbox = cv::boundingRect(m_contours[best_index]);
    //     m_detect_result.pixel_x = best_bbox.x + best_bbox.width  / 2.0f;
    //     m_detect_result.pixel_y = best_bbox.y + best_bbox.height / 2.0f;
    //     m_state = State::FOUND;
    // }
    // else
    // {
    //     m_detect_result.pixel_x = 0;
    //     m_detect_result.pixel_y = 0;
    //     m_state = State::LOST;
    // }

    // m_index++;
    // m_detect_result.index = m_index;
    // m_sum_dtMs += m_detect_result.frame_dtMs;
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
