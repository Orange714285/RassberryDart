#include "detector.hpp"
#include "camera.hpp"

Detector::Detector()
{
	m_outfile.open("data.csv");
    	if (!m_outfile) {
        	std::cerr << "无法打开文件" << std::endl;
        	return;
    	}
}
Detector::~Detector()
{
    if (m_outfile.is_open()) m_outfile.close();
}

//cv::Mat Detector::plane_to_rgb_mat(const FrameData& frame)
//{
 //   if (!frame.valid)
  //      return {};

    //int fd = frame.plane.fd.get();
    //if (fd < 0)
    //    return {};

   // auto it = m_mapped_planes.find(fd);
   /// if (it == m_mapped_planes.end())
   // {
    //    void* addr = mmap(
     //       nullptr,
      //      frame.plane.length,
       //     PROT_READ,
        //    MAP_SHARED,
         //   fd,
          //  0
       // );

        //if (addr == MAP_FAILED)
        ///{
         //   std::cerr << "mmap failed, fd = " << fd << std::endl;
         //   return {};
       // }

       // m_mapped_planes[fd] = {addr, frame.plane.length};
        //it = m_mapped_planes.find(fd);
    //}

    //uint8_t* data = static_cast<uint8_t*>(it->second.addr) + frame.plane.offset;

    //return cv::Mat(
    //    frame.height,
    //    frame.width,
     //   CV_8UC3,
      //  data,
       // frame.stride
//    );
//}
void Detector::detect_and_draw_lights(cv::Mat &frame)
{
    
    m_now = std::chrono::steady_clock::now();
    m_detect_result.frame_dtMs = std::chrono::duration_cast<std::chrono::milliseconds>(m_now - m_last).count();
    m_last = m_now;

    auto last = std::chrono::steady_clock::now();
    cv::Mat binary;
    	
    cv::Scalar lower(m_rmin, m_gmin, m_bmin);
    cv::Scalar upper(m_rmax, m_gmax, m_bmax);
    cv::inRange(frame, lower, upper, binary);//开销太大了！！！
    return ;
    auto now = std::chrono::steady_clock::now();
    auto dtMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    std::cout<<"|binary_dtMs :"<< dtMs ;
    return ;
    std::cout<< "|[WANNING] Return failed!";
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
    
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Point> best_contour;
    
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    float cur_circularity,best_circularity;
    int best_index=-1;
    best_circularity = 0;
    
    for (int i = 0; i < (int)contours.size(); i++)
    {

        cv::Rect bbox = cv::boundingRect(contours[i]); 
        cv::rectangle(frame, bbox, cv::Scalar(0, 0, 255), 1);   
        cur_circularity = (float)contourCircularity(contours[i]);
        cv::putText(frame, std::to_string(cur_circularity), cv::Point(bbox.x,bbox.y), cv::FONT_HERSHEY_SIMPLEX, 
                0.5, cv::Scalar(255,255,255), 1, cv::LINE_AA);
        if (cur_circularity >= best_circularity)
        {
            best_index = i;
            best_circularity = cur_circularity;            
        }
    }

    if(best_circularity > 0.6 )
    {
        cv::Rect best_contour_bounding_rect = cv::boundingRect(contours[best_index]);
        m_detect_result.pixel_x = best_contour_bounding_rect.x;
        m_detect_result.pixel_y = best_contour_bounding_rect.y;
        cv::rectangle(frame,best_contour_bounding_rect,cv::Scalar(0,255,0),1);
    }
    else
    {
        m_detect_result.pixel_x = 0;
        m_detect_result.pixel_y = 0;
    }
    m_index++;
    m_detect_result.index = m_index;


    m_detect_result.frame_dtMs = 0;
    if (m_outfile.is_open()) {
        m_outfile << m_detect_result.index << ","
                << m_detect_result.pixel_x << ","
                << m_detect_result.pixel_y << ",\n";
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
