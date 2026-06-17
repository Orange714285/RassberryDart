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

    Camera    ov5647;
    // Recorder  recorder;
    // Detector  detector;
    // ImageStreamer streamer;

    if (!ov5647.start())
    {
        ov5647.stop();
        return -1;
    }

    // if (!recorder.start("output.mp4", ov5647.width(), ov5647.height(), ov5647.fps()))
    // {
    //     ov5647.stop();
    //     std::cout << "[ERROR]recorder start failed!" << std::endl;
    //     return -1;
    // }

    while (g_running.load())
    {
        cv::Mat frame = ov5647.wait_and_get_latest_frame();
        if (!g_running.load())
            break;
        if (frame.empty())
        {
            std::cout << "Frame empty!" << std::endl;
            break;
        }
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

        // streamer.send(frame);
        // recorder.write_frame(frame);
        // detector.detect_and_draw_lights(frame);
    }
    // recorder.stop();
    ov5647.stop();
    return 0;
}
