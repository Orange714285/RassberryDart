#include "detector.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <opencv2/opencv.hpp>

Detector::Detector()
{
    m_state = State::LOST;
}

Detector::~Detector(){}
void Detector::detect_and_draw_lights(cv::Mat &frame)
{
    m_now = std::chrono::steady_clock::now();
    m_vision_data.m_frame_dt_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(m_now - m_last).count());
    m_last = m_now;

    set_roi(frame.size());

    // 直接从 ROI 区域提取 HSV 二值化
    cv::Mat frame_roi = frame(m_roi_rect);
    cv::Mat hsv_roi, binary;
    cv::cvtColor(frame_roi, hsv_roi, cv::COLOR_BGR2HSV);
    cv::inRange(hsv_roi,
                cv::Scalar(m_h_low,  m_s_low,  m_v_low),
                cv::Scalar(m_h_high, m_s_high, m_v_high),
                binary);

    // 查找轮廓（复用 m_contours 成员避免每帧堆分配）
    m_contours.clear();
    cv::findContours(binary, m_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double best_circularity = 0.0;
    int    best_index       = -1;
    for (int i = 0; i < static_cast<int>(m_contours.size()); i++)
    {
        cv::Rect bbox = cv::boundingRect(m_contours[i]);
        cv::Rect bbox_on_frame(
            bbox.x + m_roi_rect.x, bbox.y + m_roi_rect.y,
            bbox.width, bbox.height);
        cv::rectangle(frame, bbox_on_frame, cv::Scalar(0, 0, 255), 1);

        double cur_circularity = contourCircularity(m_contours[i]);
        cv::putText(frame, std::to_string(cur_circularity),
                    cv::Point(bbox_on_frame.x, bbox_on_frame.y),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

        if (is_contour_touch_border(m_contours[i], binary.cols, binary.rows))
            continue;
        if (cur_circularity >= best_circularity)
        {
            best_index = i;
            best_circularity = cur_circularity;
        }
    }

    if (best_circularity > m_best_circularity_standard)
    {
        cv::Rect best_bbox = cv::boundingRect(m_contours[best_index]);
        cv::Rect best_bbox_on_frame(
            best_bbox.x + m_roi_rect.x, best_bbox.y + m_roi_rect.y,
            best_bbox.width, best_bbox.height);
        m_vision_data.m_target_pixel_x = best_bbox_on_frame.x + best_bbox_on_frame.width  / 2.0;
        m_vision_data.m_target_pixel_y = best_bbox_on_frame.y + best_bbox_on_frame.height / 2.0;
        cv::rectangle(frame, best_bbox_on_frame, cv::Scalar(255, 255, 0), 1);
        m_vision_data.m_target_status = 1.0;
        m_state = State::FOUND;
    }
    else
    {
        m_vision_data.m_target_pixel_x = 0;
        m_vision_data.m_target_pixel_y = 0;
        m_vision_data.m_target_status = 0.0;
        m_state = State::LOST;
    }

    m_index++;
    cv::rectangle(frame, m_roi_rect, cv::Scalar(255, 0, 0), 1);
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

void Detector::set_hsv_params(int h_low, int h_high, int s_low, int s_high, int v_low, int v_high)
{
    m_h_low  = std::max(0,   std::min(180, h_low));
    m_h_high = std::max(0,   std::min(180, h_high));
    m_s_low  = std::max(0,   std::min(255, s_low));
    m_s_high = std::max(0,   std::min(255, s_high));
    m_v_low  = std::max(0,   std::min(255, v_low));
    m_v_high = std::max(0,   std::min(255, v_high));
}
