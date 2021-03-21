cmake_minimum_required(VERSION 3.4.1)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

add_library(native-camera2-jni SHARED 
            native-camera2-jni.cpp)
#find_library( gl-lib GLESv2 )
find_library( gl-lib2 GLESv3 )
target_link_libraries(native-camera2-jni
                      android
                      log
                        ${gl-lib2})
