cmake_minimum_required(VERSION 3.14)
project(scrcpy LANGUAGES C VERSION 1.17)

set(SCRCPY_VERSION ${PROJECT_VERSION})

option(PORTABLE "Whether should search scrcpy-server in the same dir" ON)

configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(SYSDIR ${CMAKE_CURRENT_LIST_DIR}/src/sys/win/)
else ()
    set(SYSDIR ${CMAKE_CURRENT_LIST_DIR}/src/sys/unix/)
endif ()


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
        ${SYSDIR}/command.c

        $<$<PLATFORM_ID:Windows>:
        ${SYSDIR}/getopt.c
        >
        ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

find_package(SDL2 REQUIRED)
find_package(FFMPEG COMPONENTS avformat avcodec avutil REQUIRED)

target_include_directories(scrcpy
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
        ${FFMPEG_INCLUDE_DIRS}
        ${SYSDIR}
)

target_link_libraries(scrcpy
        PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
        ${FFMPEG_LIBRARIES}
        $<$<PLATFORM_ID:Windows>:ws2_32>
)
