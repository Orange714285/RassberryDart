#include <camera.hpp>

Camera::Camera()
{
	m_width = 640;
	m_height = 480;
	m_crop_width = 1920;
	m_crop_height = 960;
	m_exposure_time_us = 1000;
	m_frame_duration_us = 16972;
	m_has_new_frame = false;
	m_stopped = false;
        m_max_queue_size = 3;
}

bool Camera::initialize()
{
	//signal(SIGINT,this->onSigInt);
	//Create and set a Camera Manager
	m_camera_manager = std::make_unique<libcamera::CameraManager>();
	m_camera_manager->start();

	//Create and acquire a camera
	for (auto const &camera : m_camera_manager->cameras())
	{
		std::cout <<"The camera id is "<< camera->id() << std::endl;
	}
	auto cameras = m_camera_manager->cameras();
	if (cameras.empty()) 
	{
    		std::cout << "No cameras were identified on the system."<< std::endl;
    		m_camera_manager->stop();
		return false;
	}
	std::string cameraId = cameras[0]->id();
	m_camera = m_camera_manager ->get(cameraId);
	m_camera -> acquire();
	
	//Configure the camera
	std::unique_ptr<libcamera::CameraConfiguration> camera_configuration = m_camera -> generateConfiguration({libcamera::StreamRole::VideoRecording});
	libcamera::StreamConfiguration &stream_configuration = camera_configuration->at(0);
	std::cout << "Default viewfinder configuration is: "<< stream_configuration.toString() << std::endl;

	//Change and validate the configuration
	stream_configuration.size.width = m_width;
	stream_configuration.size.height = m_height;
	stream_configuration.pixelFormat = libcamera::formats::RGB888;
	m_stride = stream_configuration.stride;

	camera_configuration->validate();
	std::cout << "Validated viewfinder configuration is: "<< stream_configuration.toString() << std::endl;
	
	m_camera -> configure(camera_configuration.get());
	

	//Allocate FrameBuffers
	m_frame_buffer_allocator = new libcamera::FrameBufferAllocator(m_camera);

	for (libcamera::StreamConfiguration &cfg: *camera_configuration)
	{
		int ret = m_frame_buffer_allocator -> allocate(cfg.stream());
		if (ret < 0) 
		{
        		std::cout << "Can't allocate buffers" << std::endl;
        		return false;
    		}
		size_t allocated = m_frame_buffer_allocator->buffers(cfg.stream()).size();
   		std::cout << "Allocated " << allocated << " buffers for stream" << std::endl;
	}

	//Frame Capture
	m_stream = stream_configuration.stream();
	const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers = m_frame_buffer_allocator->buffers(m_stream);

	for (unsigned int i = 0; i < buffers.size(); ++i) 
	{
    		std::unique_ptr<libcamera::Request> request = m_camera->createRequest();
    		if (!request)
    		{
        		std::cerr << "Can't create request" << std::endl;
        		return false;
    		}

		request->controls().set(libcamera::controls::AwbEnable,false);
		request->controls().set(libcamera::controls::AeEnable,false);
    		request->controls().set(libcamera::controls::ExposureTime,m_exposure_time_us);
		request->controls().set(libcamera::controls::FrameDurationLimits,libcamera::Span<const int64_t,2>({m_frame_duration_us,m_frame_duration_us}));
		if(get_crop())
			request->controls().set(libcamera::controls::ScalerCrop,m_center_crop);	
		const std::unique_ptr<libcamera::FrameBuffer> &buffer = buffers[i];
    		int ret = request->addBuffer(m_stream, buffer.get());
    		if (ret < 0)
    		{
        		std::cerr << "Can't set buffer for request"
              		<< std::endl;
   		     	return ret;
    		}
    		m_requests.push_back(std::move(request));
	}
	
	m_camera -> requestCompleted.connect(this,&Camera::request_complete);
	if(m_camera -> start())
	{
		std::cout<<"Camera start failed!"<<std::endl;
		return false;
	}
	
	int request_id=0;
	for (std::unique_ptr<libcamera::Request> &request : m_requests)
	{
		m_camera ->queueRequest(request.get());	
		request_id++;
	}
	return true;
}

bool Camera::get_crop()
{
        auto crop_max_opt = m_camera->properties().get(libcamera::properties::ScalerCropMaximum);
        if (!crop_max_opt) 
            return false;
        
        else {
                const libcamera::Rectangle crop_max = *crop_max_opt;

            	int x = crop_max.x + (int(crop_max.width)  - m_width)  / 2;
            	int y = crop_max.y + (int(crop_max.height) - m_height) / 2;

            	x &= ~1;
            	y &= ~1;
            	int w = m_crop_width  & ~1;
            	int h = m_crop_height & ~1;

            	// 边界夹紧
            	if (x < crop_max.x) 
			x = crop_max.x;
            	if (y < crop_max.y) 
			y = crop_max.y;
            	if (x + w > crop_max.x + int(crop_max.width))  
			x = crop_max.x + int(crop_max.width)  - w;
            	if (y + h > crop_max.y + int(crop_max.height)) 
			y = crop_max.y + int(crop_max.height) - h;

            	m_center_crop = libcamera::Rectangle(x, y, w, h);

            	std::cout << "[INFO] ScalerCropMaximum: "
                	  << crop_max.x << "," << crop_max.y << " "
                	  << crop_max.width << "x" << crop_max.height << "\n";
            	std::cout << "[INFO] Using center ScalerCrop: "
                          << m_center_crop.x << "," << m_center_crop.y << " "
                          << m_center_crop.width << "x" << m_center_crop.height << "\n";
		return true;
	}
}

void Camera::request_complete(libcamera::Request *request)
{
	//Camera *self = reinterpret_cast<Camera*>(request->cookie());
	if (request->status() == libcamera::Request::RequestCancelled)
   	{
		std::cout<<"Request canceled!"<<std::endl;
		return;
	}
	const std::map<const libcamera::Stream *, libcamera::FrameBuffer *> &buffers = request->buffers();	
	for (auto bufferPair : buffers) 
	{
		libcamera::FrameBuffer *buffer = bufferPair.second;
    		const libcamera::FrameMetadata &metadata = buffer->metadata();
		const libcamera::Span<const libcamera::FrameBuffer::Plane> &planes = buffer->planes();
		if (!planes.empty())
		{
			m_latest_frame.plane = planes[0];
            		m_latest_frame.width = m_width;
            		m_latest_frame.height = m_height;
            		m_latest_frame.stride = m_stride;
            		m_latest_frame.sequence = buffer->metadata().sequence;
            		m_latest_frame.valid = true;
			m_rgb_frame  = plane_to_rgb_mat(m_latest_frame);
			m_has_new_frame = true;
		}

		{
			std::lock_guard<std::mutex> lock(m_mtx);
        		if (m_frame_queue.size() >= m_max_queue_size) 
			{
            			m_frame_queue.pop_front(); 
        		}
        		m_frame_queue.push_back(m_rgb_frame.clone());
		}

		m_plane_condition_variable.notify_one();

		//std::cout << " seq: " << std::setw(6) << std::setfill('0') << metadata.sequence << " bytesused: ";
		//unsigned int nplane = 0;
		//for (const libcamera::FrameMetadata::Plane &plane : metadata.planes())
		//{
    		//	std::cout << plane.bytesused;
    		//	if (++nplane < metadata.planes().size()) 
        	//		std::cout << "/";
		//}
		//std::cout << std::endl;
		
	}
		
	request -> reuse(libcamera::Request::ReuseBuffers);
	m_camera->queueRequest(request);
}

cv::Mat Camera::wait_and_get_latest_frame()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_plane_condition_variable.wait(lock, [this]{
        return !m_frame_queue.empty() || m_stopped;
    });
    if (m_stopped) return {};
    
    cv::Mat frame = std::move(m_frame_queue.front());  
    m_frame_queue.pop_front();
    return frame;  
}

cv::Mat Camera::plane_to_rgb_mat(const FrameData& frame)
{
    if (!frame.valid)
        return {};

    int fd = frame.plane.fd.get();
    if (fd < 0)
        return {};

    auto it = m_mapped_planes.find(fd);
    if (it == m_mapped_planes.end())
    {
        void* addr = mmap(
            nullptr,
            frame.plane.length,
            PROT_READ,
            MAP_SHARED,
            fd,
            0
        );

        if (addr == MAP_FAILED)
        {
            std::cerr << "mmap failed, fd = " << fd << std::endl;
            return {};
        }

        m_mapped_planes[fd] = {addr, frame.plane.length};
        it = m_mapped_planes.find(fd);
    }
    uint8_t* data = static_cast<uint8_t*>(it->second.addr) + frame.plane.offset;

    return cv::Mat(
        frame.height,
        frame.width,
        CV_8UC3,
        data,
        frame.stride
    );
}

void Camera::stop()
{
	m_stopped = 0;
}
Camera::~Camera()
{
	std::cout<<"-------------------OVER------------------"<<std::endl;
	if (m_camera) 
	{  
        	m_camera->stop();
	}
	if (m_frame_buffer_allocator)
	{
		m_frame_buffer_allocator->free(m_stream);
		delete m_frame_buffer_allocator;
	}
	std::cout<<"Delete m_allocator successed!"<<std::endl;
	
	if (m_camera)
	{	
		m_camera->release();
       		m_camera.reset();
	}
	std::cout<<"Release camera successed!"<<std::endl;
  
	if (m_camera_manager) 
	{  
        	m_camera_manager->stop();
	}
	std::cout<<"Stop camera manager successed!"<<std::endl;
}

