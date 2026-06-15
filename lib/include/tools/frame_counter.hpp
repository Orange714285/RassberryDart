#pragma once

#include <chrono>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <string>

/**
 * @brief FPS 帧率统计工具（全静态方法，无需实例化）
 *
 * 用法：
 *   while (g_running)
 *   {
 *       FrameCounter::tick();                              // 每帧调用
 *       double fps = FrameCounter::fps();                   // 获取当前 FPS
 *       size_t total = FrameCounter::total();               // 总帧数
 *       FrameCounter::log_to_csv("fps.csv");                // 写入 YYYYMMDD_HHMMSS_fps.csv
 *   }
 */
class FrameCounter
{
public:
    using Clock = std::chrono::steady_clock;

    /** @brief 每帧调用一次 */
    static void tick()
    {
        s_total++;
        s_window_count++;

        auto now = Clock::now();
        double elapsed = std::chrono::duration<double>(now - s_window_start).count();
        if (elapsed >= s_window_sec)
        {
            s_fps = s_window_count / elapsed;
            s_window_count = 0;
            s_window_start = now;
        }
    }

    /** @brief 当前 FPS（基于最近约 1 秒窗口） */
    static double fps() { return s_fps; }

    /** @brief 自启动以来的总帧数 */
    static size_t total() { return s_total; }

    /**
     * @brief 将当前时间戳和 FPS 追加到 CSV 文件
     * @param name  基础文件名，实际写入 YYYYMMDD_HHMMSS_name
     *
     * 格式：YYYY-MM-DD HH:MM:SS, fps
     */
    static void log_to_csv(const std::string& name)
    {
        if (s_filename.empty())
        {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_r(&t, &tm);
            char ts[32];
            std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm);
            s_filename = std::string(ts) + "_" + name;
        }

        std::ofstream out(s_filename, std::ios::app);
        if (!out) return;

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);

        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << ", " << std::fixed << std::setprecision(1)
            << s_fps << "\n";
    }

private:
    inline static size_t      s_total        = 0;
    inline static size_t      s_window_count = 0;
    inline static double      s_fps          = 0.0;
    inline static Clock::time_point s_window_start = Clock::now();
    inline static double      s_window_sec   = 1.0;
    inline static std::string s_filename;
};
