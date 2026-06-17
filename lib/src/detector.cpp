#include "detector.hpp"
#include "tools/timer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>

Detector::Detector()
{
    m_state = State::LOST;
}

Detector::~Detector()
{
}

void Detector::detect_and_draw_lights(cv::Mat &bayer_frame)
{
    m_now = std::chrono::steady_clock::now();
    m_vision_data.m_frame_dt_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(m_now - m_last).count());
    m_last = m_now;

    int h = bayer_frame.rows;
    int w = bayer_frame.cols;

    // ── 先确定 ROI（基于上一帧状态） ──
    set_roi(bayer_frame.size());
    const int roi_x0 = m_roi_rect.x;
    const int roi_y0 = m_roi_rect.y;
    const int roi_x1 = roi_x0 + m_roi_rect.width;
    const int roi_y1 = roi_y0 + m_roi_rect.height;
    const int roi_w  = m_roi_rect.width;
    const int roi_h  = m_roi_rect.height;

    const int margin   = 2;
    const int min_area = 4;

    int  min_x = roi_w, min_y = roi_h, max_x = 0, max_y = 0;
    long area  = 0;
    long sum_x = 0, sum_y = 0;

    // 对每个 2x2 RGGB 块: 计算绿光差异 → 写回棋盘格 → 同时统计 ROI 内白像素
    for (int row = 0; row < h; row += 2)
    {
        uint8_t* dst0 = bayer_frame.ptr<uint8_t>(row);
        uint8_t* dst1 = bayer_frame.ptr<uint8_t>(row + 1);

        for (int col = 0; col < w; col += 2)
        {
            uint8_t r  = dst0[col];
            uint8_t g0 = dst0[col + 1];
            uint8_t g1 = dst1[col];
            uint8_t b  = dst1[col + 1];

            int g_sum  = g0 + g1;
            int rb_sum = r + b + 1;
            int ratio  = (g_sum << 8) / (rb_sum * 2);
            int diff   = ratio - 128;
            uint8_t val = (diff > m_diff_threshold) ? 255 : 0;

            dst0[col]     = 0;
            dst0[col + 1] = val;
            dst1[col]     = val;
            dst1[col + 1] = 0;

            if (val == 0) continue;

            // G₀ 位于 (col+1, row)
            {
                int gx = col + 1;
                int gy = row;
                if (gx >= roi_x0 && gx < roi_x1 && gy >= roi_y0 && gy < roi_y1)
                {
                    int rx = gx - roi_x0, ry = gy - roi_y0;
                    if (rx < min_x) min_x = rx;
                    if (rx > max_x) max_x = rx;
                    if (ry < min_y) min_y = ry;
                    if (ry > max_y) max_y = ry;
                    sum_x += rx; sum_y += ry; area++;
                }
            }

            // G₁ 位于 (col, row+1)
            {
                int gx = col;
                int gy = row + 1;
                if (gx >= roi_x0 && gx < roi_x1 && gy >= roi_y0 && gy < roi_y1)
                {
                    int rx = gx - roi_x0, ry = gy - roi_y0;
                    if (rx < min_x) min_x = rx;
                    if (rx > max_x) max_x = rx;
                    if (ry < min_y) min_y = ry;
                    if (ry > max_y) max_y = ry;
                    sum_x += rx; sum_y += ry; area++;
                }
            }
        }
    }

    bool touches_border = (min_x <= margin) || (min_y <= margin) ||
                          (max_x >= roi_w - 1 - margin) ||
                          (max_y >= roi_h - 1 - margin);

    if (area >= min_area && !touches_border)
    {
        cv::Rect best_bbox(min_x, min_y,
                           max_x - min_x + 1, max_y - min_y + 1);
        cv::Rect best_bbox_on_frame(
            best_bbox.x + roi_x0, best_bbox.y + roi_y0,
            best_bbox.width, best_bbox.height);

        m_vision_data.m_target_pixel_x = static_cast<int>(sum_x / area) + roi_x0;
        m_vision_data.m_target_pixel_y = static_cast<int>(sum_y / area) + roi_y0;
        m_vision_data.m_target_status = 1;

        cv::rectangle(bayer_frame, best_bbox_on_frame, cv::Scalar(255, 255, 0), 1);
        m_state = State::FOUND;
    }
    else
    {
        m_vision_data.m_target_pixel_x = 0;
        m_vision_data.m_target_pixel_y = 0;
        m_vision_data.m_target_status = 0;
        m_state = State::LOST;
        std::cout << "[INFO] Target lost! " << std::endl;
    }

    m_index++;

    cv::rectangle(bayer_frame, m_roi_rect, cv::Scalar(255, 255, 255), 1);
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
        const int roi_x = static_cast<int>(std::round(m_vision_data.m_target_pixel_x - m_roi_width  / 2.0));
        const int roi_y = static_cast<int>(std::round(m_vision_data.m_target_pixel_y - m_roi_height / 2.0));
        m_roi_rect = cv::Rect(roi_x, roi_y, m_roi_width, m_roi_height) & frame_rect;

        if (m_roi_rect.width <= 0 || m_roi_rect.height <= 0)
            m_roi_rect = frame_rect;
    }
    else
    {
        m_roi_rect = frame_rect;
    }
}
