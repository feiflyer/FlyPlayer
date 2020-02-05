
cmake_minimum_required(VERSION 3.4.1)

# 引入FFmpeg的头文件
include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp/include)

# 方式一
# 引入FFmpeg的库文件，设置内部的方式引入，指定库的目录是 -L  指定具体的库-l
# 这种方式方便快捷
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}")

# 方式二
#设置ffmpeg库所在路径的变量
#set(FF ${CMAKE_SOURCE_DIR}/${CMAKE_ANDROID_ARCH_ABI})
#add_library(avcodec SHARED IMPORTED)
#set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${FF}/libavcodec.so)
#
#add_library(avformat SHARED IMPORTED)
#set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${FF}/libavformat.so)
#
#add_library(avutil SHARED IMPORTED)
#set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${FF}/libavutil.so)
#
#add_library(swscale SHARED IMPORTED)
#set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${FF}/libswscale.so)
#
#add_library(swresample SHARED IMPORTED)
#set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${FF}/libswresample.so)
#
#add_library(avfilter SHARED IMPORTED)
#set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION ${FF}/libavfilter.so)

#找到包含所有的cpp文件
file(GLOB allCpp src/main/cpp/*.*)

add_library(
             # 生成的库的名字
             flyffmpeg-lib

             # 动态库
             SHARED

             # 源文件
             ${allCpp} )


target_link_libraries( #目标库
                       flyffmpeg-lib

                       # 把ffmpeg的动态库以来进来
                       avformat
                       avcodec
                       avutil
                       avfilter
                       swscale
                       swresample

                       # NDK中的log库
                       log )