cmake_minimum_required(VERSION 3.10)
project(argus_camera CXX)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_CXX_STANDARD 14)

find_package(TegraMM REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenCV COMPONENTS core highgui)
if(${TegraMM_FOUND})
  message(STATUS "Found Tegra Multimedia API")
else()
  message(STATUS "Could not find Tegra Multimedia API")
endif()

find_library(NV_JPEG nvjpeg HINTS /usr/lib/aarch64-linux-gnu/tegra/)
find_library(NV_BUF_UTILS nvbuf_utils HINTS /usr/lib/aarch64-linux-gnu/tegra/)
set(NV_LIBRARIES ${NV_JPEG} ${NV_BUF_UTILS})

include_directories(/usr/src/jetson_multimedia_api/include/ ./include)

add_library(argus_camera SHARED
  src/argus_camera.cpp
  ${TegraMM_COMMON_SOURCES}
)
target_include_directories(argus_camera PRIVATE ${TegraMM_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(argus_camera ${TegraMM_LIBRARIES} ${OpenCV_LIBRARIES} ${NV_LIBRARIES} Threads::Threads)

add_executable(repeat_example src/repeat_example.cpp)
target_include_directories(repeat_example PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(repeat_example ${OpenCV_LIBRARIES} argus_camera)

add_executable(one_shot_example src/one_shot_example.cpp)
target_include_directories(one_shot_example PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(one_shot_example ${OpenCV_LIBRARIES} argus_camera)