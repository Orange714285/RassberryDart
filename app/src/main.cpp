#include <atomic>
#include <chrono>
#include <iostream>
#include <signal.h>

#include <camera.hpp>
#include <detector.hpp>
#include <image_streamer.hpp>
#include <recorder.hpp>

#include <tools/cpu_monitor.hpp>
#include <tools/frame_counter.hpp>


static std::atomic<bool> g_running{true};
static void onSigInt(int /*signal*/)
{
    std::cout << "[INFO] Cleaning up all of resources" << std::endl;
    g_running = false;
}

int main()
{
    signal(SIGINT, onSigInt);
    Camera IMX219;
    // ImageStreamer image_streamer;
    Detector detector;
    if (!IMX219.start())
    {
        IMX219.stop();
        return -1;
    }
    // recorder.start("output",640,480,120);
    while (g_running.load())
    {
        if (!g_running.load())
            break;
        cv::Mat frame = IMX219.wait_and_get_latest_frame();
        if (frame.empty())
        {
            std::cout << "Frame empty!" << std::endl;
            break;
        }
        detector.detect_and_draw_lights(frame);
        // image_streamer.send(frame);
        // recorder.write_frame(frame);
        FrameCounter::tick();                
        if (FrameCounter::total() % 60 == 0)
        {
            CpuMonitor::sample();                   
            double cpu_pct = CpuMonitor::usage(); 
            std::cout << "[INFO] CPU usage : " << cpu_pct << "  ";
            std::cout << "[FPS] " << FrameCounter::fps()<< " | total: " << FrameCounter::total() << std::endl;
            FrameCounter::log_to_csv("fps.csv");
            CpuMonitor::log_to_csv("cpu.csv");
        }
    }
    IMX219.stop();
    // recorder.stop();
    return 0;
}
