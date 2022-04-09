// Copyright <2022> [Copyright rossihwang@gmail.com]

#pragma once

#include <opencv2/core.hpp>

#include "Argus/Argus.h"
#include "EGLStream/NV/ImageNativeBuffer.h"
#include "nvbuf_utils.h"
#include "NvBuffer.h"
#include "EGLStream/EGLStream.h"
#include "NvVideoConverter.h"

using namespace Argus;

class ArgusCamera {
 private:
  int device_id_;
  uint32_t width_;
  uint32_t height_;
  bool enable_metadata_;

  /// Producer
  UniqueObj<Argus::CaptureSession> capture_session_;
  UniqueObj<Argus::OutputStream> stream_;
  /// Consumer
  UniqueObj<EGLStream::FrameConsumer> frame_consumer_;
  /// Camera info
  std::vector<CameraDevice*> devices_;

  UniqueObj<Request> request_;
  UniqueObj<EventQueue> event_queue_;

  
 public:
  ArgusCamera(int device_id, uint32_t width, uint32_t height, bool enable_metadata = false);
  ~ArgusCamera();
  void list_devices();
  bool start_repeat();
  void stop_repeat();
  bool read(cv::OutputArray image);
  bool start_one_shot();
  bool wait_for_event();
  void set_exposure_time(uint64_t exposure_time);
  CameraDevice* get_devices(int device_id) const {
    return devices_[device_id];
  }
};

