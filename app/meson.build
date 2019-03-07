src = [
    'src/main.c',
    'src/command.c',
    'src/control_event.c',
    'src/controller.c',
    'src/convert.c',
    'src/decoder.c',
    'src/device.c',
    'src/file_handler.c',
    'src/fps_counter.c',
    'src/input_manager.c',
    'src/lock_util.c',
    'src/net.c',
    'src/recorder.c',
    'src/scrcpy.c',
    'src/screen.c',
    'src/server.c',
    'src/str_util.c',
    'src/tiny_xpm.c',
    'src/stream.c',
    'src/video_buffer.c',
]

if not get_option('crossbuild_windows')

    # native build
    dependencies = [
        dependency('libavformat'),
        dependency('libavcodec'),
        dependency('libavutil'),
        dependency('sdl2'),
    ]

else

    # cross-compile mingw32 build (from Linux to Windows)
    cc = meson.get_compiler('c')

    prebuilt_sdl2 = meson.get_cross_property('prebuilt_sdl2')
    sdl2_bin_dir = meson.current_source_dir() + '/../prebuilt-deps/' + prebuilt_sdl2 + '/bin'
    sdl2_lib_dir = meson.current_source_dir() + '/../prebuilt-deps/' + prebuilt_sdl2 + '/lib'
    sdl2_include_dir = '../prebuilt-deps/' + prebuilt_sdl2 + '/include'

    sdl2 = declare_dependency(
        dependencies: [
            cc.find_library('SDL2', dirs: sdl2_bin_dir),
            cc.find_library('SDL2main', dirs: sdl2_lib_dir),
        ],
        include_directories: include_directories(sdl2_include_dir)
    )

    prebuilt_ffmpeg_shared = meson.get_cross_property('prebuilt_ffmpeg_shared')
    prebuilt_ffmpeg_dev = meson.get_cross_property('prebuilt_ffmpeg_dev')
    ffmpeg_bin_dir = meson.current_source_dir() + '/../prebuilt-deps/' + prebuilt_ffmpeg_shared + '/bin'
    ffmpeg_include_dir = '../prebuilt-deps/' + prebuilt_ffmpeg_dev + '/include'
    ffmpeg = declare_dependency(
        dependencies: [
            cc.find_library('avcodec-58', dirs: ffmpeg_bin_dir),
            cc.find_library('avformat-58', dirs: ffmpeg_bin_dir),
            cc.find_library('avutil-56', dirs: ffmpeg_bin_dir),
        ],
        include_directories: include_directories(ffmpeg_include_dir)
    )

    dependencies = [
        ffmpeg,
        sdl2,
        cc.find_library('mingw32')
    ]

endif

cc = meson.get_compiler('c')

if host_machine.system() == 'windows'
    src += [ 'src/sys/win/command.c' ]
    src += [ 'src/sys/win/net.c' ]
    dependencies += cc.find_library('ws2_32')
else
    src += [ 'src/sys/unix/command.c' ]
    src += [ 'src/sys/unix/net.c' ]
endif

conf = configuration_data()

# expose the build type
conf.set('BUILD_DEBUG', get_option('buildtype') == 'debug')

# the version, updated on release
conf.set_quoted('SCRCPY_VERSION', meson.project_version())

# the prefix used during configuration (meson --prefix=PREFIX)
conf.set_quoted('PREFIX', get_option('prefix'))

# the path of the server, which will be appended to the prefix
# ignored if OVERRIDE_SERVER_PATH if defined
# must be consistent with the install_dir in server/meson.build
conf.set_quoted('PREFIXED_SERVER_PATH', '/share/scrcpy/scrcpy-server.jar')

# the path of the server to be used "as is"
# this is useful for building a "portable" version (with the server in the same
# directory as the client)
override_server_path = get_option('override_server_path')
if override_server_path != ''
    conf.set_quoted('OVERRIDE_SERVER_PATH', override_server_path)
else
    # undefine it
    conf.set('OVERRIDE_SERVER_PATH', false)
endif

# the default client TCP port for the "adb reverse" tunnel
# overridden by option --port
conf.set('DEFAULT_LOCAL_PORT', '27183')

# the default max video size for both dimensions, in pixels
# overridden by option --max-size
conf.set('DEFAULT_MAX_SIZE', '0')  # 0: unlimited

# the default video bitrate, in bits/second
# overridden by option --bit-rate
conf.set('DEFAULT_BIT_RATE', '8000000')  # 8Mbps

# whether the app should always display the most recent available frame, even
# if the previous one has not been displayed
# SKIP_FRAMES improves latency at the cost of framerate
conf.set('SKIP_FRAMES', get_option('skip_frames'))

# enable High DPI support
conf.set('HIDPI_SUPPORT', get_option('hidpi_support'))

# disable console on Windows
conf.set('WINDOWS_NOCONSOLE', get_option('windows_noconsole'))

configure_file(configuration: conf, output: 'config.h')

src_dir = include_directories('src')

if get_option('windows_noconsole')
    c_args = [ '-mwindows' ]
    link_args = [ '-mwindows' ]
else
    c_args = []
    link_args = []
endif

executable('scrcpy', src, dependencies: dependencies, include_directories: src_dir, install: true, c_args: c_args, link_args: link_args)


### TESTS

tests = [
    ['test_control_event_queue', ['tests/test_control_event_queue.c', 'src/control_event.c']],
    ['test_control_event_serialize', ['tests/test_control_event_serialize.c', 'src/control_event.c']],
    ['test_strutil', ['tests/test_strutil.c', 'src/str_util.c']],
]

foreach t : tests
    exe = executable(t[0], t[1], include_directories: src_dir, dependencies: dependencies)
    test(t[0], exe)
endforeach
