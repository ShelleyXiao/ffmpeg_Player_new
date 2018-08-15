
cmake_minimum_required(VERSION 3.4.1)



set(PATH_TO_NATIVE src/main/cpp)


file(GLOB FILES_AUDIO ${PATH_TO_NATIVE}/audio/*.cpp)
file(GLOB FILES_VIDEO ${PATH_TO_NATIVE}/video/*.cpp)
file(GLOB FILES_COMMON_UTILS ${PATH_TO_NATIVE}/common/*.cpp)
file(GLOB FILES_COMMON_OPENSL ${PATH_TO_NATIVE}/common/opensl_media/*.cpp)


include_directories(src/main/cpp/include)
include_directories(src/main/cpp/audio)
include_directories(src/main/cpp/video)
include_directories(src/main/cpp/common)

add_library(
             zxplayer

             SHARED

             src/main/cpp/ZXVideoPlayer.cpp
             src/main/cpp/base_player.cpp
             src/main/cpp/video_player_control.cpp
             src/main/cpp/JavaJNICallback.cpp
             src/main/cpp/WlPlayStatus.cpp

             ${FILES_AUDIO}
             ${FILES_VIDEO}
             ${FILES_COMMON_UTILS}
             ${FILES_COMMON_OPENSL}
             )

#添加libavcodec-57.so
add_library( avcodec-57
             SHARED
             IMPORTED)
set_target_properties( avcodec-57
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavcodec-57.so)


#添加libavdevice-57.so
add_library( avdevice-57
             SHARED
             IMPORTED)
set_target_properties( avdevice-57
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavdevice-57.so)

add_library( avfilter-6
             SHARED
             IMPORTED)
set_target_properties( avfilter-6
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavfilter-6.so)

add_library( avformat-57
             SHARED
             IMPORTED)
set_target_properties( avformat-57
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavformat-57.so)

add_library( avutil-55
             SHARED
             IMPORTED)
set_target_properties( avutil-55
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavutil-55.so)

add_library( swresample-2
             SHARED
             IMPORTED)
set_target_properties( swresample-2
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libswresample-2.so)

add_library( swscale-4
             SHARED
             IMPORTED)
set_target_properties( swscale-4
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libswscale-4.so)

add_library( postproc-54
             SHARED
             IMPORTED)
set_target_properties( postproc-54
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libpostproc-54.so)



target_link_libraries( # Specifies the target library.
                       zxplayer
                       avcodec-57
                       avdevice-57
                       avfilter-6
                       avformat-57
                       avutil-55
                       swresample-2
                       swscale-4
                       postproc-54
                       OpenSLES
                       android
                       log
                        )