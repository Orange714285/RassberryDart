#include "detector.hpp"

Detector::Detector()
{
    this->m_max_val = 30;
    this->m_kernel_close_size  = 15;
    this->m_kernel_open_size = 3;
    this->m_sum_dtMs= 0;
    this->m_roi_width = 200;
    this->m_roi_height = 200;

    this->m_outfile.open("data.csv");
    if (!m_outfile) {
        std::cerr << "无法打开文件" << std::endl;
        return;
    }
    m_outfile << "Index , pixel_x , pixel_y,dt_Ms\n";
    m_state = State::LOST;
}

Detector::~Detector()
{
    if (m_outfile.is_open()) m_outfile.close();
}

void Detector::detect_and_draw_lights(cv::Mat &frame)
{

    m_now = std::chrono::steady_clock::now();
    m_detect_result.frame_dtMs = std::chrono::duration_cast<std::chrono::milliseconds>(m_now - m_last).count();
    m_last = m_now;
    cv::Mat frame_process = frame.clone();

    // 设置roi
    set_roi(frame_process.size(), true);
    // std::cout << " |roi_x:"<<m_roi_rect.x<<" |roi_y:"<<m_roi_rect.y<<" |roi_width:"<<m_roi_rect.width<<" |roi_height:"<<m_roi_rect.height<<std::endl;    
    cv::Mat frame_roi = frame_process(m_roi_rect);
    frame_process = frame_roi; 
    
    // 高斯模糊
    cv::Size gaussian_kernel = cv::Size(7,7);
    cv::GaussianBlur(frame_process,frame_process,gaussian_kernel,0,0);

        
    //提取绿色通道
    frame_process.convertTo(frame_process,-1,1.0,0);
    std::vector<cv::Mat> channels;
    cv::split(frame_process,channels);
    frame_process = channels[1];
   	
    // 二值化
    cv::threshold(frame_process,frame_process,this->m_max_val,255,cv::THRESH_BINARY);
    
    int border = 10;
    cv::copyMakeBorder(
        frame_process,
        frame_process,
        border, border, border, border,  
        cv::BORDER_CONSTANT,                 
        cv::Scalar(0)                        
    );

    // 膨胀腐蚀
    cv::Mat kernel_close = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(this->m_kernel_close_size, this->m_kernel_open_size));
    cv::Mat kernel_open  = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(this -> m_kernel_open_size,this->m_kernel_open_size));
    cv::morphologyEx(frame_process, frame_process, cv::MORPH_OPEN, kernel_open, cv::Point(-1, -1), 1);
    cv::morphologyEx(frame_process, frame_process, cv::MORPH_CLOSE, kernel_close, cv::Point(-1, -1), 1);
    // cv::morphologyEx(binary_roi, binary_roi, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
    frame_process = frame_process(cv::Rect(border, border, frame_roi.cols, frame_roi.rows));
    
    // 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Point> best_contour;
    cv::findContours(frame_process, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    float cur_circularity,best_circularity;
    int best_index=-1;
    best_circularity = 0;    

    for (int i = 0; i < (int)contours.size(); i++)
    {
        cv::Rect bbox = cv::boundingRect(contours[i]); 
        cv::Rect bbox_on_frame(bbox.x + m_roi_rect.x, bbox.y + m_roi_rect.y, bbox.width, bbox.height);
        cv::rectangle(frame, bbox_on_frame, cv::Scalar(0,0,255), 1); 
        cur_circularity = (float)contourCircularity(contours[i]);
        cv::putText(frame, std::to_string(cur_circularity), cv::Point(bbox_on_frame.x, bbox_on_frame.y), cv::FONT_HERSHEY_SIMPLEX, 
                0.5, cv::Scalar(255,255,255), 1, cv::LINE_AA);
        if(Detector::is_contour_touch_border(contours[i], frame_process.cols, frame_process.rows)) 
            continue;
        if (cur_circularity >= best_circularity)
        {
            best_index = i;
            best_circularity = cur_circularity;            
        }
    }

    if(best_circularity > 0.65 )
    {
        cv::Rect best_contour_bounding_rect = cv::boundingRect(contours[best_index]);
        cv::Rect best_contour_rect_on_frame(
            best_contour_bounding_rect.x + m_roi_rect.x,
            best_contour_bounding_rect.y + m_roi_rect.y,
            best_contour_bounding_rect.width,
            best_contour_bounding_rect.height
        );
        m_detect_result.pixel_x = best_contour_rect_on_frame.x + best_contour_rect_on_frame.width / 2.0f;
        m_detect_result.pixel_y = best_contour_rect_on_frame.y + best_contour_rect_on_frame.height / 2.0f;
        // cv::rectangle(frame_roi,best_contour_bounding_rect,cv::Scalar(0,0,0),1);
        cv::rectangle(frame, best_contour_rect_on_frame, cv::Scalar(255,255,0), 1); 
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
    m_sum_dtMs += m_detect_result.frame_dtMs;
    cv::putText(frame, "Time:"+std::to_string(m_sum_dtMs)+"Ms", cv::Point(30, 30), cv::FONT_HERSHEY_SIMPLEX, 
                0.5, cv::Scalar(255,255,255), 1, cv::LINE_AA);    
    cv::rectangle(frame, m_roi_rect, cv::Scalar(255,0,0), 1); 
 
    if (m_outfile.is_open()) 
    {
        m_outfile << m_detect_result.index << ","
                  << m_detect_result.pixel_x << ","
                  << m_detect_result.pixel_y << ","
                  << m_detect_result.frame_dtMs<< ",\n";
    }
}

void Detector::set_roi(const cv::Size& frame_size, bool is_set_roi)
{
    if(!is_set_roi) 
        return;

    if (frame_size.width <= 0 || frame_size.height <= 0)
    {
        m_roi_rect = cv::Rect();
        return;
    }

    const cv::Rect frame_rect(0, 0, frame_size.width, frame_size.height);

    if(m_state == State::LOST)
    {
        m_roi_rect = frame_rect;
    }
    else if (m_state == State::FOUND)
    {
        const int roi_width = m_roi_width;
        const int roi_height = m_roi_height;
        const int roi_x = static_cast<int>(std::round(m_detect_result.pixel_x - roi_width / 2.0f));
        const int roi_y = static_cast<int>(std::round(m_detect_result.pixel_y - roi_height / 2.0f));
        m_roi_rect = cv::Rect(roi_x, roi_y, roi_width, roi_height) & frame_rect;

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
    int margin = 3;
    // 1. 获取轮廓的外接矩形（最小包围框）
    cv::Rect rect = cv::boundingRect(contour);

    // 2. 判断是否触达任意边缘（含安全边距）
    bool touch_left = (rect.x <= margin);                  // 左边缘
    bool touch_right = (rect.x + rect.width >= img_width - margin);  // 右边缘
    bool touch_top = (rect.y <= margin);                   // 上边缘
    bool touch_bottom = (rect.y + rect.height >= img_height - margin); // 下边缘

    // 任意一边碰到即判定为粘连
    return touch_left || touch_right || touch_top || touch_bottom;
}
