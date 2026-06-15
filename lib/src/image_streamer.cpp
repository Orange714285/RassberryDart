#include <image_streamer.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <config.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <vector>

// ============================================================================
// ★ 调参区 —— 参数值见 config/config.hpp
// ============================================================================

static constexpr const char* DEFAULT_STREAM_HOST = config::STREAM_HOST;
static constexpr int         DEFAULT_STREAM_PORT = config::STREAM_PORT;
static constexpr int         JPEG_QUALITY        = config::JPEG_QUALITY;
static constexpr size_t      MAX_FRAME_BYTES     = 10 * 1024 * 1024; // 单帧最大字节

// ============================================================================
// 协议常量（通常无需修改）
// ============================================================================

static constexpr size_t HEADER_SIZE = 4;  // 4 字节长度头 (uint32_t)

// ============================================================================
// 构造 / 析构
// ============================================================================

ImageStreamer::ImageStreamer()
    : m_mode(Mode::Silent)
{
    const char* host = std::getenv("STREAM_HOST");
    if (!host || host[0] == '\0')
        host = DEFAULT_STREAM_HOST;

    const char* port_str = std::getenv("STREAM_PORT");
    int         port     = port_str ? std::atoi(port_str) : DEFAULT_STREAM_PORT;

    initClient(host, port);
}

ImageStreamer::ImageStreamer(int port)
    : m_mode(Mode::Server)
{
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0) {
        std::cerr << "[ImageStreamer] socket() failed: " << std::strerror(errno) << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[ImageStreamer] bind() failed: " << std::strerror(errno) << std::endl;
        close(m_listen_fd);
        m_listen_fd = -1;
        return;
    }

    if (listen(m_listen_fd, 1) < 0) {
        std::cerr << "[ImageStreamer] listen() failed: " << std::strerror(errno) << std::endl;
        close(m_listen_fd);
        m_listen_fd = -1;
        return;
    }

    std::cout << "[ImageStreamer] Server listening on port " << port << std::endl;

    m_running = true;
    m_server_thread = std::thread(&ImageStreamer::serverLoop, this);
}

ImageStreamer::ImageStreamer(const std::string& server_ip, int port)
    : m_mode(Mode::Client)
{
    initClient(server_ip, port);
}

ImageStreamer::~ImageStreamer()
{
    m_running = false;
    if (m_server_thread.joinable())
        m_server_thread.join();
    closeAll();
    std::cout << "[ImageStreamer] Destroyed" << std::endl;
}

// ============================================================================
// 客户端连接
// ============================================================================

void ImageStreamer::initClient(const std::string& server_ip, int port)
{
    m_client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_client_fd < 0) {
        std::cerr << "[ImageStreamer] socket() failed: " << std::strerror(errno) << std::endl;
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "[ImageStreamer] Invalid IP: " << server_ip << std::endl;
        close(m_client_fd);
        m_client_fd = -1;
        return;
    }

    if (connect(m_client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[ImageStreamer] connect() to " << server_ip << ":"
                  << port << " failed: " << std::strerror(errno) << std::endl;
        close(m_client_fd);
        m_client_fd = -1;
        return;
    }

    int flag = 1;
    setsockopt(m_client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    m_mode      = Mode::Client;
    m_connected = true;
    std::cout << "[ImageStreamer] Connected to " << server_ip << ":" << port << std::endl;
}

// ============================================================================
// send() —— 对外唯一接口
// ============================================================================

void ImageStreamer::send(const cv::Mat& frame)
{
    if (m_mode != Mode::Client)
        return;

    if (!m_connected || m_client_fd < 0 || frame.empty())
        return;

    std::vector<uint8_t> jpeg_buf;
    std::vector<int>     params = {cv::IMWRITE_JPEG_QUALITY, JPEG_QUALITY};

    if (!cv::imencode(".jpg", frame, jpeg_buf, params)) {
        std::cerr << "[ImageStreamer] JPEG encoding failed" << std::endl;
        return;
    }

    if (jpeg_buf.size() > MAX_FRAME_BYTES) {
        std::cerr << "[ImageStreamer] Frame too large: " << jpeg_buf.size() << " bytes" << std::endl;
        return;
    }

    uint32_t size_be = htonl(static_cast<uint32_t>(jpeg_buf.size()));
    if (!sendAll(&size_be, HEADER_SIZE)) {
        m_connected = false;
        return;
    }

    if (!sendAll(jpeg_buf.data(), jpeg_buf.size())) {
        m_connected = false;
        return;
    }
}

// ============================================================================
// 服务器接收与显示循环
// ============================================================================

void ImageStreamer::serverLoop()
{
    while (m_running) {
        std::cout << "[ImageStreamer] Waiting for client connection..." << std::endl;

        sockaddr_in client_addr{};
        socklen_t   addr_len = sizeof(client_addr);

        int fd = accept(m_listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (fd < 0) {
            if (m_running)
                std::cerr << "[ImageStreamer] accept() failed: " << std::strerror(errno) << std::endl;
            break;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::cout << "[ImageStreamer] Client connected: " << ip_str << std::endl;

        int flag = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        m_client_fd = fd;
        m_connected = true;

        while (m_running) {
            uint32_t size_be = 0;
            if (!recvAll(&size_be, HEADER_SIZE)) {
                std::cerr << "[ImageStreamer] Client disconnected (header)" << std::endl;
                break;
            }

            uint32_t jpeg_size = ntohl(size_be);
            if (jpeg_size == 0 || jpeg_size > MAX_FRAME_BYTES) {
                std::cerr << "[ImageStreamer] Invalid frame size: " << jpeg_size << std::endl;
                break;
            }

            std::vector<uint8_t> jpeg_buf(jpeg_size);
            if (!recvAll(jpeg_buf.data(), jpeg_size)) {
                std::cerr << "[ImageStreamer] Client disconnected (data)" << std::endl;
                break;
            }

            cv::Mat frame = cv::imdecode(jpeg_buf, cv::IMREAD_COLOR);
            if (!frame.empty()) {
                cv::imshow(WINDOW_NAME, frame);
                cv::waitKey(1);
            }
        }

        m_connected = false;
        if (m_client_fd >= 0) {
            close(m_client_fd);
            m_client_fd = -1;
        }
        cv::destroyWindow(WINDOW_NAME);
    }
}

// ============================================================================
// Socket 工具函数
// ============================================================================

bool ImageStreamer::sendAll(const void* data, size_t len)
{
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = ::send(m_client_fd, ptr, remaining, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        ptr       += n;
        remaining -= n;
    }
    return true;
}

bool ImageStreamer::recvAll(void* data, size_t len)
{
    char*  ptr       = static_cast<char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = ::recv(m_client_fd, ptr, remaining, 0);
        if (n <= 0)
            return false;
        ptr       += n;
        remaining -= n;
    }
    return true;
}

void ImageStreamer::closeAll()
{
    if (m_client_fd >= 0) {
        close(m_client_fd);
        m_client_fd = -1;
    }
    if (m_listen_fd >= 0) {
        close(m_listen_fd);
        m_listen_fd = -1;
    }
    if (m_mode == Mode::Server)
        cv::destroyWindow(WINDOW_NAME);
}
