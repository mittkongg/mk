cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_CXX_FLAGS "-std=c++11 -g ${CMAKE_CXX_FLAGS}")
message(STATUS "optional:-std=c++11 -g ")

find_library( log-lib log )
find_library( camera-lib camera2ndk )
find_library( media-lib mediandk )
find_library( android-lib android )
find_library( gl-lib GLESv2 )

add_library(native-camera2-jni SHARED
            gl_helper.cpp
            cam_utils.cpp
            native-camera2-jni.cpp)

target_link_libraries(native-camera2-jni
                      ${log-lib} ${camera-lib} ${media-lib} ${android-lib} ${gl-lib})
