#pragma once

#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * @brief CPU 占用率监控（全静态方法，无需实例化）
 *
 * 通过多次读取 /proc/stat 的滑动窗口计算最近 0.5s 的平均占用率。
 *
 * 用法：
 *   CpuMonitor::sample();                     // 初始采样（建立基准）
 *   // ... 主循环每帧调一次 ...
 *   CpuMonitor::sample();
 *   double usage = CpuMonitor::usage();        // 最近 0.5s 平均占用率 (%)
 *   CpuMonitor::log_to_csv("cpu.csv");         // 写入 20260611_173000_cpu.csv
 */
class CpuMonitor
{
public:
    /** @brief 读取 /proc/stat 并存入滑动窗口 */
    static void sample()
    {
        std::ifstream stat("/proc/stat");
        if (!stat)
        {
            s_last_usage = -1.0;
            return;
        }

        std::string cpu_label;
        size_t user, nice, system, idle, iowait, irq, softirq, steal;
        stat >> cpu_label
             >> user >> nice >> system >> idle
             >> iowait >> irq >> softirq >> steal;

        size_t total = user + nice + system + idle + iowait + irq + softirq + steal;
        size_t idle_total = idle + iowait;  // iowait 也视为空闲

        auto now = std::chrono::steady_clock::now();
        s_history.push_back({now, total, idle_total});

        // 限制历史长度，防止无限增长
        while (s_history.size() > s_max_history)
            s_history.pop_front();
    }

    /**
     * @brief 返回最近 0.5s 的平均 CPU 占用率 (0~100)
     *
     * 从滑动窗口中剔除超过 500ms 的旧采样点，
     * 用最早有效点和最新点的计数差值计算平均占用率。
     * 若窗口内不足 2 个采样点，返回 0。
     */
    static double usage()
    {
        if (s_history.size() < 2)
            return 0.0;

        auto now = std::chrono::steady_clock::now();
        constexpr auto window = std::chrono::milliseconds(500);

        // 剔除窗口外的旧采样点（保留至少 2 个）
        while (s_history.size() > 2)
        {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - s_history.front().timestamp);
            if (age > window)
                s_history.pop_front();
            else
                break;
        }

        if (s_history.size() < 2)
            return 0.0;

        const auto& oldest = s_history.front();
        const auto& newest = s_history.back();

        size_t delta_total = newest.total - oldest.total;
        size_t delta_idle  = newest.idle  - oldest.idle;

        if (delta_total == 0)
            return 0.0;

        s_last_usage = (1.0 - static_cast<double>(delta_idle) / delta_total) * 100.0;
        return s_last_usage;
    }

    /**
     * @brief 将当前时间戳和 CPU 占用率追加到 CSV 文件
     * @param name  基础文件名，实际写入 YYYYMMDD_HHMMSS_name
     *
     * 格式：YYYY-MM-DD HH:MM:SS, usage%
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
            << usage() << "%\n";   // 使用滑动窗口平均值
    }

private:
    struct Snapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        size_t total;
        size_t idle;
    };

    inline static std::deque<Snapshot> s_history;
    inline static double s_last_usage  = -1.0;
    inline static std::string s_filename;

    // 800Hz × 1s = 800 个采样点足够覆盖 0.5s 窗口
    static constexpr size_t s_max_history = 1024;
};
