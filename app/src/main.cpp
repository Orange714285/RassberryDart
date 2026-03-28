#include <atomic>
#include <chrono>

#include <camera.hpp>
#include <detector.hpp>
#include <recorder.hpp>
static std::atomic<bool> g_running{true};
static void onSigInt(int signal)
{
	std::cout << "[INFO]Cleaning up all of resources" << std::endl;
	g_running = false;

}

static FILE* openFfmpegPipe()
{
    // 默认走硬编 h264_v4l2m2m；如果你系统没有该编码器，改成 libx264（软编）
    std::ostringstream cmd;
    cmd
      << "ffmpeg -hide_banner -loglevel error -y "
      << "-f rawvideo -pix_fmt bgr24 "
      << "-s " << 640 << "x" << 480 << " "
      << "-r " << 60 << " "
      << "-i - "
      << "-an "
      << "-c:v h264_v4l2m2m "
      << "-b:v 4M "
      << "-pix_fmt yuv420p "
      << "output.mp4";

    FILE* fp = popen(cmd.str().c_str(), "w");
    if (!fp) {
        std::cerr << "popen(ffmpeg) failed: " << std::strerror(errno) << "\n";
        std::cerr << "Command: " << cmd.str() << "\n";
    } else {
        std::cout << "FFmpeg cmd: " << cmd.str() << "\n";
    }
    return fp;
}

static void closeFfmpegPipe(FILE* fp)
{
    if (fp) {
        fflush(fp);  // 确保缓冲区中的数据被写入
        pclose(fp);  // 关闭管道
    }
    fp = nullptr;
}

Camera ov5647;
//Recorder recorder;
Detector detector;
int main()
{	
	signal(SIGINT, onSigInt);
	
	if(!ov5647.initialize())
	{
		return -1;
	}
	
        FILE* ff = openFfmpegPipe();
        if (!ff) 
	{
            ov5647.stop();
            return 1;
        }

//	if(!recorder.start("output.mp4",640,480,60))
//	{
//		std::cout<<"[ERROR]recorder start failed!"<<std::endl;
//		return -1;
//	}
	while(g_running.load())
	{
		
	//获取最新帧
        auto start = std::chrono::high_resolution_clock::now();
	cv::Mat frame = ov5647.wait_and_get_latest_frame();
        if (!g_running.load())
            break;
	if (frame.empty())
	{
		std::cout<<"Frame empty!"<<std::endl;
		break;
	}
        auto end = std::chrono::high_resolution_clock::now();
        auto get_frame_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        detector.detect_and_draw_lights(frame);
        //std::cout << "|seq = " << ov5647.m_latest_frame.sequence
        //          << "| fd = " << frame_data.plane.fd.get()
        //          << "| get_frame_duration_us = "<< get_frame_duration_us
        //          << std::endl;
	//recorder.write_frame(frame);
        
	const size_t bytes = (size_t)frame.total() * frame.elemSize();
        const size_t w = fwrite(frame.data, 1, bytes, ff);
        if (w != bytes) 
	{
        	std::cerr << "fwrite to ffmpeg failed (broken pipe?) wrote=" << w << "/" << bytes << "\n";
        	g_running = false;
        }
	
	}
	
	//recorder.stop();
	ov5647.stop();
	
	return 0;
}	
