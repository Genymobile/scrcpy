cmake_minimum_required(VERSION 3.14)
project(scrcpy C)

configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_executable(scrcpy
        src/main.c
        src/cli.c
        src/command.c
        src/control_msg.c
        src/controller.c
        src/decoder.c
        src/device.c
        src/device_msg.c
        src/event_converter.c
        src/file_handler.c
        src/fps_counter.c
        src/input_manager.c
        src/opengl.c
        src/receiver.c
        src/recorder.c
        src/scrcpy.c
        src/screen.c
        src/server.c
        src/stream.c
        src/tiny_xpm.c
        src/video_buffer.c
        src/util/net.c
        src/util/str_util.c
        $<$<PLATFORM_ID:Windows>:
        ${CMAKE_CURRENT_LIST_DIR}/src/sys/win/getopt.c
        >)

find_package(SDL2 CONFIG REQUIRED)
find_package(FFMPEG COMPONENTS avformat avcodec avutil REQUIRED)

target_include_directories(scrcpy
        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${FFMPEG_INCLUDE_DIRS}
        $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_LIST_DIR}/src/sys/win/>)
target_link_libraries(scrcpy SDL2::SDL2 SDL2::SDL2main ${FFMPEG_LIBRARIES})