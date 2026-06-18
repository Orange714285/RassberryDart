#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <string>

/**
 * @brief 目标像素坐标 CSV 日志工具（全静态方法）
 *
 * 用法：
 *   DataLogger::log(320, 240, 1);  // 写入一行: 时间戳, x, y, status
 */
class DataLogger
{
public:
    /** @brief 将目标坐标写入 data.csv（带系统时间戳） */
    static void log(int target_x, int target_y, int target_status)
    {
        if (s_filename.empty())
        {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_r(&t, &tm);
            char ts[32];
            std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm);
            s_filename = std::string(ts) + "_data.csv";
        }

        std::ofstream out(s_filename, std::ios::app);
        if (!out) return;

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);

        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << ", " << target_x
            << ", " << target_y
            << ", " << target_status << "\n";
    }

private:
    inline static std::string s_filename;
};
