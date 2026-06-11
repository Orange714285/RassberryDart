#pragma once

#include <opencv2/core/mat.hpp>

#include <atomic>
#include <string>
#include <thread>

/**
 * @brief 基于 TCP Socket 的实时图像传输与显示工具
 *
 * ── 树莓派端（一行接入） ──────────────────────────
 *   ImageStreamer streamer;
 *   streamer.send(frame);            // 不想回传？删掉这行即可
 *
 * ── Ubuntu 端（接收显示） ──────────────────────────
 *   ImageStreamer server(8080);      // 后台线程监听并自动弹出窗口显示
 *
 * 所有可调参数集中在 image_streamer.cpp 顶部的「调参区」。
 */
class ImageStreamer
{
public:
    // ────────────────────────────────────────────
    // 构造函数
    // ────────────────────────────────────────────

    /**
     * @brief 默认构造（树莓派端推荐用法）
     *
     * 自动连接 image_streamer.cpp 中配置的 DEFAULT_STREAM_HOST。
     * 环境变量 STREAM_HOST / STREAM_PORT 可临时覆盖。
     */
    ImageStreamer();

    /** 服务器模式: 监听端口，接收并显示图像 (Ubuntu 端使用) */
    explicit ImageStreamer(int port);

    /** 显式客户端模式: 直接指定目标 IP 和端口 */
    ImageStreamer(const std::string& server_ip, int port);

    ~ImageStreamer();

    // 禁止拷贝
    ImageStreamer(const ImageStreamer&) = delete;
    ImageStreamer& operator=(const ImageStreamer&) = delete;

    // ────────────────────────────────────────────
    // 对外唯一操作接口
    // ────────────────────────────────────────────

    /** @brief 发送一帧图像到远端实时显示 */
    void send(const cv::Mat& frame);

    /// 查询连接状态
    bool isConnected() const { return m_connected.load(); }

private:
    enum class Mode { Server, Client, Silent };

    Mode                  m_mode        = Mode::Silent;
    int                   m_listen_fd   = -1;
    int                   m_client_fd   = -1;
    std::thread           m_server_thread;
    std::atomic<bool>     m_running     {false};
    std::atomic<bool>     m_connected   {false};

    static constexpr const char* WINDOW_NAME = "LightDetect - Remote View";

    void serverLoop();
    void initClient(const std::string& host, int port);
    bool sendAll(const void* data, size_t len);
    bool recvAll(void* data, size_t len);
    void closeAll();
};
