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
private:
    FILE* m_fp = nullptr;
public:
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    bool write_frame(const cv::Mat& frame);
};

