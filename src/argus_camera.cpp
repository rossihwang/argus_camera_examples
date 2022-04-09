// Copyright <2022> [Copyright rossihwang@gmail.com]

#include <argus_camera.hpp>
#include <NvLogging.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

ArgusCamera::ArgusCamera(int device_id, uint32_t width, uint32_t height, bool enable_metadata)
  : device_id_(device_id),
    width_(width),
    height_(height),
    enable_metadata_(enable_metadata) {
  Argus::Status status;

  /// Get camera provider
  auto camera_provider = UniqueObj<CameraProvider>(CameraProvider::create(NULL));
  auto camera_provider_inf = interface_cast<ICameraProvider>(camera_provider);
  if (!camera_provider_inf) {
    throw std::runtime_error("ICameraProvider");
  }
  /// Get camera devices
  status = camera_provider_inf->getCameraDevices(&devices_);
  if (status != Argus::STATUS_OK) {
    throw std::runtime_error("getCameraDevices");
  }

  auto camera_device = devices_[device_id_];

  /// Create capture session
  capture_session_ = UniqueObj<CaptureSession>(camera_provider_inf->createCaptureSession(camera_device));
  auto capture_session_inf = interface_cast<ICaptureSession>(capture_session_);
  if (!capture_session_inf) {
    throw std::runtime_error("ICaptureSession");
  }
  /// Create stream settings
  auto output_stream_settings = UniqueObj<OutputStreamSettings>(capture_session_inf->createOutputStreamSettings(STREAM_TYPE_EGL));
  auto output_stream_settings_inf = interface_cast<IEGLOutputStreamSettings>(output_stream_settings);
  if (!output_stream_settings_inf) {
    throw std::runtime_error("IEGLOutputStreamSettings");
  }

  /// Set stream pixel format and resolution
  output_stream_settings_inf->setPixelFormat(Argus::PIXEL_FMT_YCbCr_420_888);
  output_stream_settings_inf->setResolution(Argus::Size2D<uint32_t>(width_, height_));
  output_stream_settings_inf->setMetadataEnable(enable_metadata_);
  /// Create stream
  stream_ = UniqueObj<OutputStream>(capture_session_inf->createOutputStream(output_stream_settings.get(), &status));
  auto stream_inf = interface_cast<IEGLOutputStream>(stream_);
  if (!stream_inf) {
    throw std::runtime_error("IEGLOutputStream");
  }

  /// Create frame consumer
  frame_consumer_ = UniqueObj<EGLStream::FrameConsumer>(EGLStream::FrameConsumer::create(stream_.get()));
  auto frame_consumer_inf = interface_cast<EGLStream::IFrameConsumer>(frame_consumer_);
  if (!frame_consumer_inf) {
    throw std::runtime_error("EGLSream::IFrameConsumer");
  }

  /// Setting default request
  /// refer to: https://docs.nvidia.com/jetson/l4t-multimedia/namespaceArgus.html#a7cb2f42916e6a666dcb04171bee1d38a
  request_ = UniqueObj<Request>(capture_session_inf->createRequest(CAPTURE_INTENT_MANUAL));
  auto request_inf = interface_cast<IRequest>(request_);
  if (!request_inf) {
    throw std::runtime_error("IRequest");
  }

  /// Enable output stream
  request_inf->enableOutputStream(stream_.get());

  /// Configure source settings in request
  /// 1. Set sensor mode
  auto camera_properties_inf = interface_cast<ICameraProperties>(devices_[device_id_]);
  std::vector<SensorMode*> sensor_modes;
  status = camera_properties_inf->getAllSensorModes(&sensor_modes);
  if (status != Argus::STATUS_OK) {
    throw std::runtime_error("getAllSensorModes");
  }
  INFO_MSG("Sensor modes: " << sensor_modes.size());

  /// TODO: Choose sensor mode smartly
  auto source_settings_inf = interface_cast<ISourceSettings>(request_inf->getSourceSettings());
  status = source_settings_inf->setSensorMode(sensor_modes[2]);
  if (status != Argus::STATUS_OK) {
    throw std::runtime_error("setSensorMode");
  }

  /// 2. Set frame duration
  constexpr uint64_t kOneSecondNanoS = 1000000000;
  constexpr int kFps = 30; 
  status = source_settings_inf->setFrameDurationRange(Argus::Range<uint64_t>(kOneSecondNanoS / kFps, kOneSecondNanoS / kFps));
  if (status != Argus::STATUS_OK) {
    throw std::runtime_error("setFrameDurationRange");
  }

  /// Configure stream settings
  auto stream_settings_inf = interface_cast<IStreamSettings>(request_inf->getStreamSettings(stream_.get()));
  if (!stream_settings_inf) {
    throw std::runtime_error("IStreamSettings");
  }
  /// Set stream resolution
  status = stream_settings_inf->setSourceClipRect(Argus::Rectangle<float>(0.0, 0.0, 1.0, 1.0));
  if (status != Argus::STATUS_OK) {
    throw std::runtime_error("setSourceClipRect");
  }

  /// Setting event queue
  auto event_provider_inf = interface_cast<IEventProvider>(capture_session_);
  if (!event_provider_inf) {
    throw std::runtime_error("IEventProvider");
  }
  
  std::vector<EventType> event_types;
  event_types.push_back(EVENT_TYPE_CAPTURE_COMPLETE);
  event_queue_ = UniqueObj<EventQueue>(event_provider_inf->createEventQueue(event_types));

  INFO_MSG("Initialize succeed!");
}

ArgusCamera::~ArgusCamera() {
  auto stream_inf = interface_cast<IEGLOutputStream>(stream_);

  stop_repeat();

  if (stream_inf) {
    stream_inf->disconnect();
  }
}

void ArgusCamera::list_devices() {
  printf("Devices size: %ld\n", devices_.size());
}

bool ArgusCamera::start_repeat() {
  Argus::Status status;

  auto capture_session_inf = interface_cast<ICaptureSession>(capture_session_);
  if (!capture_session_inf) {
    ERROR_MSG("ICaptureSession");
    return false;
  }

  /// Start repeating caputre request
  status = capture_session_inf->repeat(request_.get());
  if (status != Argus::STATUS_OK) {
    ERROR_MSG("repeat");
    return false;
  }

  /// Connect stream
  auto stream_inf = interface_cast<IEGLOutputStream>(stream_);
  stream_inf->waitUntilConnected();
}

void ArgusCamera::stop_repeat() {
  auto capture_session_inf = interface_cast<ICaptureSession>(capture_session_);
  if (capture_session_inf) {
    capture_session_inf->stopRepeat();
    capture_session_inf->waitForIdle();
  }
}

bool ArgusCamera::read(cv::OutputArray cv_image) {
  auto frame_consumer_inf = interface_cast<EGLStream::IFrameConsumer>(frame_consumer_);
  auto frame = UniqueObj<EGLStream::Frame>(frame_consumer_inf->acquireFrame());
  auto frame_inf = interface_cast<EGLStream::IFrame>(frame);
  if (!frame_inf) {
    ERROR_MSG("IFrame");
    return false;
  }

  if (enable_metadata_) {
    EGLStream::IArgusCaptureMetadata* argus_capture_metadata_inf = interface_cast<EGLStream::IArgusCaptureMetadata>(frame);
    CaptureMetadata* metadata = argus_capture_metadata_inf->getMetadata();
    if (metadata) {
      auto metadata_inf = interface_cast<ICaptureMetadata>(metadata);
      uint64_t exposure_time = metadata_inf->getSensorExposureTime();
      INFO_MSG("ExposureTime(Metadata in frame): " << static_cast<int>(exposure_time));
    } else {
      WARN_MSG("No metadata available");
    }
  }

  auto image = frame_inf->getImage();
  int fd = -1;
  auto native_buffer_inf = interface_cast<EGLStream::NV::IImageNativeBuffer>(image);
  auto resolution = Argus::Size2D<uint32_t>(width_, height_);

  /// ref: https://www.e-consystems.com/articles/camera/detailed_guide_to_libargus_with_surveilsquad.asp
  fd = native_buffer_inf->createNvBuffer(resolution, NvBufferColorFormat_ABGR32, NvBufferLayout_Pitch);
  void *data = NULL;  /// virtual addr
  NvBufferMemMap(fd, 0, NvBufferMem_Read, &data);
  NvBufferMemSyncForCpu(fd, 0, &data);  /// sync data from device to cpu

  cv::Mat buffer = cv::Mat(resolution.height(), resolution.width(), CV_8UC4, data);
  cv_image.create(height_, width_, CV_8UC3);
  cv::Mat out = cv_image.getMat();
  // cv::Mat test;
  cv::cvtColor(buffer, out, cv::COLOR_RGBA2BGR);
  // cv::imshow("test", test);
  // cv::waitKey(0);
  NvBufferMemUnMap(fd, 0, &data);
}

bool ArgusCamera::start_one_shot() {
  
  auto capture_session_inf = interface_cast<ICaptureSession>(capture_session_);
  if (!capture_session_inf) {
    ERROR_MSG("ICaptureSession");
    return false;
  }

  capture_session_inf->capture(request_.get());

  return true;
}

bool ArgusCamera::wait_for_event() {
  uint64_t next_exposure_time;
  auto event_provider_inf = interface_cast<IEventProvider>(capture_session_);
  if (!event_provider_inf) {
    return false;
  }
  Argus::Status status = event_provider_inf->waitForEvents(event_queue_.get());
  if (status != Argus::STATUS_OK) {
    return false;
  }

  auto event_queue_inf = interface_cast<IEventQueue>(event_queue_);
  uint32_t num_events = event_queue_inf->getSize();
  INFO_MSG("Event amount: " << num_events);
  auto event_inf = interface_cast<const IEventCaptureComplete>(event_queue_inf->getEvent(num_events - 1));
  auto metadata = event_inf->getMetadata();
  if (metadata) {
    auto metadata_inf = interface_cast<const ICaptureMetadata>(metadata);
    next_exposure_time = metadata_inf->getSensorExposureTime();
    INFO_MSG("ExposureTime(Metadata in event): " << static_cast<int>(next_exposure_time)); 
  } else {
    WARN_MSG("No metadata available");
    return false;
  }

  /// Update request with respect to metadata
  auto request_inf = interface_cast<IRequest>(request_);
  if (!request_inf) {
    ERROR_MSG("IRequest");
    return false;
  }

  auto source_settings_inf = interface_cast<ISourceSettings>(request_inf->getSourceSettings());
  if (next_exposure_time < 1000000) {
    next_exposure_time = 33330000;
  }
  status = source_settings_inf->setExposureTimeRange(Argus::Range<uint64_t>(next_exposure_time - 1000000));
  if (status != Argus::STATUS_OK) {
    WARN_MSG("setExposureTimeRange");
    return false;
  }

  /// Start new repeat
  auto capture_session_inf = interface_cast<ICaptureSession>(capture_session_);
  if (!capture_session_inf) {
    ERROR_MSG("ICaptureSession");
    return false;
  }

  /// Start repeating caputre request
  status = capture_session_inf->repeat(request_.get());  /// Updated request
  if (status != Argus::STATUS_OK) {
    ERROR_MSG("repeat");
    return false;
  }

  return true;
}