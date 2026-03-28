#include <camera.hpp>

Camera::Camera()
{	
    	std::ifstream config_file("../config/config.json");
    	if (!config_file.is_open()) {
        	throw std::runtime_error("无法打开配置文件 config.json");
    	}

	nlohmann::json root_json = nlohmann::json::parse(config_file);

    	if (!root_json.contains("camera")) {
        	throw std::runtime_error("配置文件缺少 camera 节点");
    	}
    	const nlohmann::json& camera_json = root_json["camera"];

    	m_width                  = camera_json.value("m_width", 640);
        m_height                 = camera_json.value("m_height", 480);
        m_crop_width             = camera_json.value("m_crop_width", 1920);
        m_crop_height            = camera_json.value("m_crop_height", 960);
   	m_exposure_time_us       = camera_json.value("m_exposure_time_us", 1000);
    	m_frame_duration_us      = camera_json.value("m_frame_duration_us", 16972);
    	m_has_new_frame          = camera_json.value("m_has_new_frame", false);
    	m_stopped                = camera_json.value("m_stopped", false);
    	m_max_queue_size         = camera_json.value("m_max_queue_size", 3);
	m_colour_temperature      = camera_json.value("m_colour_temperature",6100);

        std::cout << "\n===== camera config loaded =====" << std::endl;
        std::cout << std::boolalpha;
        std::cout << "width:                  " << m_width                  << std::endl;
        std::cout << "height:                 " << m_height                 << std::endl;
        std::cout << "crop_width:             " << m_crop_width             << std::endl;
        std::cout << "crop_height:            " << m_crop_height            << std::endl;
        std::cout << "exposure_time_us:       " << m_exposure_time_us       << std::endl;
        std::cout << "frame_duration_us:      " << m_frame_duration_us      << std::endl;
        std::cout << "has_new_frame:          " << m_has_new_frame          << std::endl;
        std::cout << "stopped:                " << m_stopped                << std::endl;
        std::cout << "max_queue_size:         " << m_max_queue_size         << std::endl;
	std::cout << "colour temperature      " << m_colour_temperature      << std::endl;
	std::cout << "==================================" << std::endl;
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
		request->controls().set(libcamera::controls::ColourTemperature,m_colour_temperature);
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

	if (m_index == 0 )
	{
		const libcamera::ControlList &meta = request->metadata();
		std::cout << "============ Camera's real parameters: ============="<<std::endl;
		if (auto gains = meta.get(libcamera::controls::ColourGains))
    		{
    		    std::cout << "[INFO] ColourGains: R=" << (*gains)[0]
    		              << "|B=" << (*gains)[1] <<std::endl;
    		}
    		else
    		{
    		    std::cout << "ColourGains not available" << std::endl;
    		}		

    		if (auto ct = meta.get(libcamera::controls::ColourTemperature))
    		{
    		    std::cout << "[INFO] ColourTemperature: " << *ct << " K" << std::endl ;
    		}
    		else
    		{
    		    std::cout << "ColourTemperature not available" << std::endl;
    		}

    		if (auto exposure_time = meta.get(libcamera::controls::ExposureTime))
  	        {
    		    std::cout << "[INFO] ExposureTime:" << *exposure_time <<std::endl;
    		}
   		else
    		{
		    std::cout << "ExposureTime not available" <<std::endl;
    		}
		
		if (auto crop = meta.get(libcamera::controls::ScalerCrop))
		{
		    std::cout << "[INFO ] ScalerCrop: "
        		      << "x=" << crop->x
		              << ", y=" << crop->y
		              << ", w=" << crop->width
		              << ", h=" << crop->height;
		}
		else
		{
		    std::cout << "|ScalerCrop not available";
		}

    		std::cout<<std::endl;
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
	}
		
	request -> reuse(libcamera::Request::ReuseBuffers);
	m_camera->queueRequest(request);
	m_index ++ ;
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

void Camera::print_camera_parameter()
{
	
}

Camera::~Camera()
{
	std::cout<<"------------------CAMERA--OVER------------------"<<std::endl;
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



