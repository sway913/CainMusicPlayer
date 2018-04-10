# 设置cmake最低版本
cmake_minimum_required(VERSION 3.4.1)

# 设置GCC编译器的编译选项
if(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations -Wmissing-prototypes")
endif(CMAKE_COMPILER_IS_GNUCC)

# 设置clang编译器的编译选项
if(CMAKE_COMPILER_IS_CLANG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations -Wmissing-prototypes -Wdocumentation -Wunreachable-code")
endif(CMAKE_COMPILER_IS_CLANG)

# 设置路径
set(distribution_DIR ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI})

# 加载ffmpeg库
add_library( ffmpeg
             SHARED
             IMPORTED )
set_target_properties( ffmpeg
                       PROPERTIES IMPORTED_LOCATION
                       ${distribution_DIR}/libffmpeg.so )

# 添加头文件
include_directories( src/main/cpp/ffmpeg/include
                     src/main/cpp/SoundTouch/include
                     src/main/cpp/SoundTouch )
# 添加源文件
add_library( soundtouch

             STATIC

             # library
             src/main/cpp/SoundTouch/sources/AAFilter.cpp
             src/main/cpp/SoundTouch/sources/BPMDetect.cpp
             src/main/cpp/SoundTouch/sources/cpu_detect_x86.cpp
             src/main/cpp/SoundTouch/sources/FIFOSampleBuffer.cpp
             src/main/cpp/SoundTouch/sources/FIRFilter.cpp
             src/main/cpp/SoundTouch/sources/InterpolateCubic.cpp
             src/main/cpp/SoundTouch/sources/InterpolateLinear.cpp
             src/main/cpp/SoundTouch/sources/InterpolateShannon.cpp
             src/main/cpp/SoundTouch/sources/mmx_optimized.cpp
             src/main/cpp/SoundTouch/sources/PeakFinder.cpp
             src/main/cpp/SoundTouch/sources/RateTransposer.cpp
             src/main/cpp/SoundTouch/sources/SoundTouch.cpp
             src/main/cpp/SoundTouch/sources/sse_optimized.cpp
             src/main/cpp/SoundTouch/sources/TDStretch.cpp

             # wrapper
             src/main/cpp/SoundTouch/SoundTouchWrapper.cpp
             )

find_library( log-lib log )

# 链接静态库
target_link_libraries( soundtouch

                       -landroid
                       ${log-lib} )

# 添加源文件
add_library( cainmusicplayer

             SHARED
             # common
             src/main/cpp/common/Looper.cpp
             src/main/cpp/common/Mutex.cpp
             src/main/cpp/common/Thread.cpp

             # music player
             src/main/cpp/CainMusicPlayer/AudioOutput.cpp
             src/main/cpp/CainMusicPlayer/Clock.cpp
             src/main/cpp/CainMusicPlayer/FrameQueue.cpp
             src/main/cpp/CainMusicPlayer/MediaDecoder.cpp
             src/main/cpp/CainMusicPlayer/MusicPlayer.cpp
             src/main/cpp/CainMusicPlayer/PacketQueue.cpp

             src/main/cpp/MusicPlayerController.cpp
             )

find_library( OpenSLES-lib OpenSLES )

# 链接动态库
target_link_libraries( cainmusicplayer

                       ffmpeg
                       soundtouch

                       -landroid
                       ${log-lib}
                       ${OpenSLES-lib} )
