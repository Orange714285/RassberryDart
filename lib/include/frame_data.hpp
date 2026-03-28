#pragma once

#include <libcamera/libcamera.h>
#include <cstdint>

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

