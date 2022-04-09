// Copyright <2022> [Copyright rossihwang@gmail.com]

#include <argus_camera.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

int main(int argc, char** argv) {

  auto camera_ptr = std::make_unique<ArgusCamera>(0, 1280, 720, true);
  camera_ptr->list_devices();
  camera_ptr->start_repeat();

  cv::Mat frame;
  for (;;) {
    camera_ptr->wait_for_event();
    camera_ptr->read(frame);
    if (frame.empty()) {
      std::cout << "Empty frame" << std::endl;
      break;
    }
    cv::imshow("argus_camera_viewer", frame);
    auto ret = cv::waitKey(33);
    if (ret == 'q') {
      break;
    }
  }
  std::cout << "Exit" << std::endl;
  return 0;
}