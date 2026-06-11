#pragma once

#include <chrono>
#include <iostream>
#include <string>

/**
 * @brief 代码耗时测量工具（全静态方法，无需实例化）
 *
 * 用法 A —— start/stop：
 *   Timer::start("my_func");
 *   // ... 被测代码 ...
 *   Timer::stop();
 *
 * 用法 B —— measure（推荐）：
 *   Timer::measure("my_func", [&]{
 *       // ... 被测代码 ...
 *   });
 *
 * 注意：start/stop 不允许多层嵌套，如需嵌套请用 measure。
 */
class Timer
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // ────────────────────────────────────────────
    // 风格 A：手动 start / stop
    // ────────────────────────────────────────────

    /** @brief 开始计时。若已处于计时状态则报错退出。 */
    static void start(const std::string& name)
    {
        if (s_running)
        {
            std::cerr << "[Timer] ERROR: already timing \"" << s_name
                      << "\", start(\"" << name
                      << "\") rejected (nesting not supported)" << std::endl;
            return;
        }
        s_name    = name;
        s_start   = Clock::now();
        s_running = true;
    }

    /** @brief 停止计时并打印耗时（毫秒） */
    static void stop()
    {
        if (!s_running)
        {
            std::cerr << "[Timer] ERROR: stop() called without matching start()" << std::endl;
            return;
        }
        auto end = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - s_start).count();
        std::cout << "[Timer] " << s_name << ": " << ms << " ms" << std::endl;
        s_running = false;
    }

    // ────────────────────────────────────────────
    // 风格 B：measure lambda（支持嵌套）
    // ────────────────────────────────────────────

    /**
     * @brief 测量一段代码耗时并打印。
     * @param name  标签名
     * @param fn    待测代码（lambda / 函数对象）
     */
    template <typename Fn>
    static void measure(const std::string& name, Fn&& fn)
    {
        auto t0 = Clock::now();
        fn();
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "[Timer] " << name << ": " << ms << " ms" << std::endl;
    }

private:
    inline static std::string s_name;
    inline static TimePoint   s_start{};
    inline static bool        s_running = false;
};
