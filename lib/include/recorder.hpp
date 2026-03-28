#pragma once
#include <opencv2/opencv.hpp>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct AnnotatedFrame
{
    cv::Mat image;         
    uint64_t sequence = 0;
    bool valid = false;    
};

class Recorder
{
public:
    Recorder() = default;
    ~Recorder();

    bool start(const std::string& output_path, int width, int height, int fps);
    void stop();

    void publish_frame(const cv::Mat& frame, uint64_t sequence);

private:
    void record_loop();
    bool write_frame(const cv::Mat& frame);

private:
    FILE* m_pipe = nullptr;

    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;

    std::thread m_thread;
    std::mutex m_mtx;
    std::condition_variable m_cv;

    AnnotatedFrame m_latest_frame;
    bool m_has_new_frame = false;

    std::atomic<bool> m_running{false};
};
