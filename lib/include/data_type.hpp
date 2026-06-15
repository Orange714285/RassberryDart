# pragma once
#include <iostream>
#include <string_view>
#include <cstdint>
#include <libcamera/libcamera.h>

struct VisionData
{
    int m_target_pixel_x{0};
    int m_target_pixel_y{0};
    int m_target_status{0};
    int m_frame_dt_ms{0};
};

struct FrameData
{
    libcamera::FrameBuffer::Plane plane;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int stride = 0;
    uint64_t sequence = 0;
    bool valid = false;

    FrameData():plane{libcamera::SharedFD(-1),0,0}{}
};

class CarData
{
public:
    float acc_frd_x_mg;
    float acc_frd_y_mg;
    float acc_frd_z_mg;
    float gyro_frd_x_dps;
    float gyro_frd_y_dps;
    float gyro_frd_z_dps;
    float roll;
    float yaw;
    float pitch;
    float index;
    double m_cpu_usage;
    double m_fps;

    int m_target_pixel_x;
    int m_target_pixel_y;
    int m_target_status;
    int  m_frame_dt_ms;

    void data_update(float acc_x, float acc_y, float acc_z,
                   float gyro_x, float gyro_y, float gyro_z,
                   float r, float p, float y, float idx,double cpu_usage,double fps)
    {
        acc_frd_x_mg  = acc_x;
        acc_frd_y_mg  = acc_y;
        acc_frd_z_mg  = acc_z;
        gyro_frd_x_dps = gyro_x;
        gyro_frd_y_dps = gyro_y;
        gyro_frd_z_dps = gyro_z;
        roll  = r;
        pitch = p;
        yaw   = y;
        index = idx;
        m_cpu_usage = cpu_usage;
        m_fps = fps;
    }

    void vision_update(const VisionData& v)
    {
        m_target_pixel_x = v.m_target_pixel_x;
        m_target_pixel_y = v.m_target_pixel_y;
        m_target_status  = v.m_target_status;
        m_frame_dt_ms    = v.m_frame_dt_ms;
    }

    void print(std::string_view state_label) const
    {
        std::cout << "\033[H\033[J";
        std::cout << "============================" << std::endl;
        std::cout << "  [" << state_label << "]" << std::endl;
        std::cout << "============================" << std::endl;
        std::cout << "[INFO] acc_frd_x_mg: "  << acc_frd_x_mg  << std::endl;
        std::cout << "[INFO] acc_frd_y_mg: "  << acc_frd_y_mg  << std::endl;
        std::cout << "[INFO] acc_frd_z_mg: "  << acc_frd_z_mg  << std::endl;
        std::cout << "[INFO] gyr_frd_x_dps: " << gyro_frd_x_dps << std::endl;
        std::cout << "[INFO] gyr_frd_y_dps: " << gyro_frd_y_dps << std::endl;
        std::cout << "[INFO] gyr_frd_z_dps: " << gyro_frd_z_dps << std::endl;
        std::cout << "[INFO] roll:  " << roll  << std::endl;
        std::cout << "[INFO] pitch: " << pitch << std::endl;
        std::cout << "[INFO] yaw:   " << yaw   << std::endl;
        std::cout << "[INFO] index: " << index << std::endl;
        std::cout << "============================" << std::endl;
        std::cout << std::flush;
    }
};