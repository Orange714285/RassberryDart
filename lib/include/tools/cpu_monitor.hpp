#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * @brief CPU 占用率监控（全静态方法，无需实例化）
 *
 * 通过两次读取 /proc/stat 的差值计算占用率。
 *
 * 用法：
 *   CpuMonitor::sample();                     // 初始采样
 *   // ... 主循环每帧调一次 ...
 *   CpuMonitor::sample();
 *   double usage = CpuMonitor::usage();        // 获取当前占用率 (%)
 *   CpuMonitor::log_to_csv("cpu.csv");         // 写入 20260611_173000_cpu.csv
 */
class CpuMonitor
{
public:
    /** @brief 读取 /proc/stat 并计算当前 CPU 占用率 */
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

        if (s_has_prev)
        {
            size_t delta_total = total - s_prev_total;
            size_t delta_idle  = idle_total - s_prev_idle;
            s_last_usage = (delta_total > 0)
                ? (1.0 - static_cast<double>(delta_idle) / delta_total) * 100.0
                : 0.0;
        }
        else
        {
            s_last_usage = 0.0;   // 首次采样，尚无差值
        }

        s_prev_total = total;
        s_prev_idle  = idle_total;
        s_has_prev   = true;
    }

    /** @brief 返回最近一次 sample() 计算的 CPU 占用率 (0~100)，未采样则返回 -1 */
    static double usage() { return s_last_usage; }

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
            << s_last_usage << "%\n";
    }

private:
    inline static size_t s_prev_total  = 0;
    inline static size_t s_prev_idle   = 0;
    inline static double s_last_usage  = -1.0;
    inline static bool   s_has_prev    = false;
    inline static std::string s_filename;
};
