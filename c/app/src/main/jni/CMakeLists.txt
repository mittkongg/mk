cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_CXX_FLAGS "-std=c++11 -g ${CMAKE_CXX_FLAGS}")
message(STATUS "optional:-std=c++11 -g ")

find_library( log-lib log )
find_library( camera-lib camera2ndk )
find_library( media-lib mediandk )
find_library( android-lib android )

find_library( gl3-lib GLESv3 )
find_library( egl-lib EGL )

add_library(native-camera SHARED
            native-camera.cpp)

target_link_libraries(native-camera
                      ${log-lib} ${android-lib} libGLESv3.so ${egl-lib})
