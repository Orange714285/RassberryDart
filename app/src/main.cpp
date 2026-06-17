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
    // ImageStreamer image_streamer;
    // Recorder recorder;
    std::cout << "3425" <<std::endl;
    signal(SIGINT, onSigInt);
    Camera ov5647;
    Detector detector;
    if (!ov5647.start())
    {
        ov5647.stop();
        return -1;
    }
    // recorder.start("output",640,480,120);
    while (g_running.load())
    {
        if (!g_running.load())
            break;
        cv::Mat frame = ov5647.wait_and_get_latest_frame();
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
    ov5647.stop();
    // recorder.stop();
    return 0;
}
