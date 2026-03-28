#include "recorder.hpp"

#include <iostream>

Recorder::~Recorder()
{
    stop();
}

bool Recorder::start(const std::string& output_path, int width, int height, int fps)
{
    if (m_running.load())
        return false;

    m_width = width;
    m_height = height;
    m_fps = fps;

    std::string cmd =
        "ffmpeg -y "
        "-f rawvideo "
        "-pix_fmt bgr24 "
        "-s " + std::to_string(m_width) + "x" + std::to_string(m_height) + " "
        "-r " + std::to_string(m_fps) + " "
        "-i - "
        "-an "
        "-c:v h264_v4l2m2m "
        "-b:v 4M "
        "\"" + output_path + "\"";

    m_pipe = popen(cmd.c_str(), "w");
    if (!m_pipe)
        return false;

    m_running.store(true);
    m_thread = std::thread(&Recorder::record_loop, this);
    return true;
}

void Recorder::stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);
    m_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    if (m_pipe)
    {
        pclose(m_pipe);
        m_pipe = nullptr;
    }
}

void Recorder::publish_frame(const cv::Mat& frame, uint64_t sequence)
{
    if (!m_running.load())
        return;

    std::lock_guard<std::mutex> lock(m_mtx);

    m_latest_frame.image = frame.clone();
    m_latest_frame.sequence = sequence;
    m_latest_frame.valid = true;

    m_has_new_frame = true;
    m_cv.notify_one();
}

void Recorder::record_loop()
{
    while (m_running.load())
    {
        AnnotatedFrame frame;

        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_cv.wait(lock, [this] {
                return m_has_new_frame || !m_running.load();
            });

            if (!m_running.load())
                break;

            frame = m_latest_frame;
            m_has_new_frame = false;
        }

        if (frame.valid)
        {
            if (!write_frame(frame.image))
            {
                std::cerr << "Recorder write_frame failed at seq = "
                          << frame.sequence << std::endl;
            }
        }
    }
}

bool Recorder::write_frame(const cv::Mat& frame)
{
    if (!m_pipe || frame.empty())
        return false;

    if (frame.cols != m_width || frame.rows != m_height)
        return false;

    if (frame.type() != CV_8UC3)
        return false;

    cv::Mat continuous = frame.isContinuous() ? frame : frame.clone();
    size_t bytes = continuous.total() * continuous.elemSize();

    size_t written = fwrite(continuous.data, 1, bytes, m_pipe);
    return written == bytes;
}
