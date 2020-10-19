cmake_minimum_required(VERSION 3.10)
project(NiScrcpy)
set(CMAKE_CXX_STANDARD 14)

include_directories(/mingw64/include)
include_directories(./)
link_directories(/usr/local/Cellar/ffmpeg/4.2.1/lib/)

include_directories(/usr/local/Cellar/sdl2/2.0.10/include/)
link_directories(/usr/local/Cellar/sdl2/2.0.10/lib)

aux_source_directory(. SRC_LIST) # 搜索当前目录下的所有.cpp文件


add_executable(NiScrcpy ${SRC_LIST})

target_link_libraries(
        NiScrcpy
        avcodec
        avdevice
        avfilter
        avformat
        avresample
        avutil
        postproc
        swresample
        swscale
        SDL2
)