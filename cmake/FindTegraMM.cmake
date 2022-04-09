set(TegraMM_ROOT /usr/src/jetson_multimedia_api)
set(TegraMM_FOUND FALSE)

if(EXISTS ${TegraMM_ROOT})
  # set packages
  set(TegraMM_INCLUDE_DIRS ${TegraMM_ROOT}/include ${TegraMM_ROOT}/include/libjpeg-8b /usr/include/libdrm)
  set(TegraMM_INCLUDES ${TegraMM_INCLUDE_DIRS})
  
  find_library(ARGUS_LIBRARY NAMES nvargus HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra)
  find_library(V4L2_LIBRARY NAMES v4l2 HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra)
  find_library(EGL_LIBRARY EGL HINTS /usr/lib/aarch64-linux-gnu/tegra)
  find_library(DRM_LIBRARY drm HINTS /usr/lib/aarch64-linux-gnu/tegra)

  set(TegraMM_LIBRARY_DIRS /usr/lib/aarch64-linux-gnu/tegra /usr/lib/aarch64-linux-gnu)
  set(TegraMM_LIBRARIES ${ARGUS_LIBRARY} ${V4L2_LIBRARY} ${EGL_LIBRARY} ${DRM_LIBRARY} X11 GLESv2)
  file(GLOB TegraMM_COMMON_SOURCES ${TegraMM_ROOT}/samples/common/classes/*.cpp)

  include_directories(${TegraMM_INCLUDE_DIRS})
  link_directories(${TegraMM_LIBRARY_DIRS})
  set(TegraMM_FOUND TRUE)
endif()

