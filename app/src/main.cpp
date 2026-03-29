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

Camera ov5647;
Recorder recorder;
Detector detector;
int main()
{	
	signal(SIGINT, onSigInt);
	
	if(!ov5647.initialize())
	{
		ov5647.stop();
		return -1;
	}
	
	if(!recorder.start("output.mp4",640,480,60))
	{
		ov5647.stop();
		std::cout<<"[ERROR]recorder start failed!"<<std::endl;
		return -1;
	}

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
	recorder.write_frame(frame);
        
	}
	
	recorder.stop();
	ov5647.stop();
	
	return 0;
}	
