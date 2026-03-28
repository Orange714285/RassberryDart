#include "recorder.hpp"

#include <iostream>

Recorder::~Recorder()
{
    stop();
}

bool Recorder::start(const std::string& output_path, int width, int height, int fps)
{
    m_width = width;
    m_height = height;
    m_fps = fps;
    std::ostringstream cmd;
    cmd
      << "ffmpeg -hide_banner -loglevel error -y "
      << "-f rawvideo -pix_fmt bgr24 "
      << "-s " << m_width << "x" << m_height << " "
      << "-r " << m_fps << " "
      << "-i - "
      << "-an "
      << "-c:v h264_v4l2m2m "
      << "-b:v 4M "
      << "-pix_fmt yuv420p "
      << "output.mp4";

    m_fp = popen(cmd.str().c_str(), "w");
    if (!m_fp) {
        std::cerr << "popen(ffmpeg) failed: " << std::strerror(errno) << "\n";
        std::cerr << "Command: " << cmd.str() << "\n";
	return false;
    } else {
        std::cout << "FFmpeg cmd: " << cmd.str() << "\n";
    	return true;	
    }
    
}

void Recorder::stop()
{
   if (m_fp) 
   {
        fflush(m_fp);  // 确保缓冲区中的数据被写入
        pclose(m_fp);  // 关闭管道
   }
   m_fp = nullptr;
}

bool Recorder::write_frame(const cv::Mat& frame)
{
    if (!m_fp || frame.empty())
    {
	    std::cout<<"Can't not find m_fp or frame!"<<std::endl;
    	    return false;
    }

    if (frame.cols != m_width || frame.rows != m_height)
    {
	std::cout<<"Frame size != setting size!"<<std::endl;	
        return false;
    }
    if (frame.type() != CV_8UC3)
    {
	std::cout<<"Frame type is not CV_8UC3!"<<std::endl;
        return false;
    }	
    const size_t bytes = (size_t)frame.total() * frame.elemSize();
    size_t written = fwrite(frame.data, 1, bytes, m_fp);
    return written == bytes;
}

