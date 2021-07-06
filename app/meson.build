src = [
    'src/main.c',
    'src/adb.c',
    'src/cli.c',
    'src/clock.c',
    'src/compat.c',
    'src/control_msg.c',
    'src/controller.c',
    'src/decoder.c',
    'src/device_msg.c',
    'src/event_converter.c',
    'src/file_handler.c',
    'src/fps_counter.c',
    'src/frame_buffer.c',
    'src/input_manager.c',
    'src/opengl.c',
    'src/receiver.c',
    'src/recorder.c',
    'src/scrcpy.c',
    'src/screen.c',
    'src/server.c',
    'src/stream.c',
    'src/tiny_xpm.c',
    'src/video_buffer.c',
    'src/util/log.c',
    'src/util/net.c',
    'src/util/process.c',
    'src/util/str_util.c',
    'src/util/thread.c',
    'src/util/tick.c',
]

if host_machine.system() == 'windows'
    src += [ 'src/sys/win/process.c' ]
else
    src += [ 'src/sys/unix/process.c' ]
endif

v4l2_support = host_machine.system() == 'linux'
if v4l2_support
    src += [ 'src/v4l2_sink.c' ]
endif

check_functions = [
    'strdup'
]

cc = meson.get_compiler('c')

if not get_option('crossbuild_windows')

    # native build
    dependencies = [
        dependency('libavformat'),
        dependency('libavcodec'),
        dependency('libavutil'),
        dependency('sdl2'),
    ]

    if v4l2_support
        dependencies += dependency('libavdevice')
    endif

else

    # cross-compile mingw32 build (from Linux to Windows)
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

if host_machine.system() == 'windows'
    dependencies += cc.find_library('ws2_32')
endif

conf = configuration_data()

foreach f : check_functions
  if cc.has_function(f)
    define = 'HAVE_' + f.underscorify().to_upper()
    conf.set(define, true)
  endif
endforeach

# the version, updated on release
conf.set_quoted('SCRCPY_VERSION', meson.project_version())

# the prefix used during configuration (meson --prefix=PREFIX)
conf.set_quoted('PREFIX', get_option('prefix'))

# build a "portable" version (with scrcpy-server accessible from the same
# directory as the executable)
conf.set('PORTABLE', get_option('portable'))

# the default client TCP port range for the "adb reverse" tunnel
# overridden by option --port
conf.set('DEFAULT_LOCAL_PORT_RANGE_FIRST', '27183')
conf.set('DEFAULT_LOCAL_PORT_RANGE_LAST', '27199')

# the default video bitrate, in bits/second
# overridden by option --bit-rate
conf.set('DEFAULT_BIT_RATE', '8000000')  # 8Mbps

# run a server debugger and wait for a client to be attached
conf.set('SERVER_DEBUGGER', get_option('server_debugger'))

# select the debugger method ('old' for Android < 9, 'new' for Android >= 9)
conf.set('SERVER_DEBUGGER_METHOD_NEW', get_option('server_debugger_method') == 'new')

# enable V4L2 support (linux only)
conf.set('HAVE_V4L2', v4l2_support)

configure_file(configuration: conf, output: 'config.h')

src_dir = include_directories('src')

executable('scrcpy', src,
           dependencies: dependencies,
           include_directories: src_dir,
           install: true,
           c_args: [])

install_man('scrcpy.1')


### TESTS

# do not build tests in release (assertions would not be executed at all)
if get_option('buildtype') == 'debug'
    tests = [
        ['test_buffer_util', [
            'tests/test_buffer_util.c'
        ]],
        ['test_cbuf', [
            'tests/test_cbuf.c',
        ]],
        ['test_cli', [
            'tests/test_cli.c',
            'src/cli.c',
            'src/util/str_util.c',
        ]],
        ['test_clock', [
            'tests/test_clock.c',
            'src/clock.c',
        ]],
        ['test_control_msg_serialize', [
            'tests/test_control_msg_serialize.c',
            'src/control_msg.c',
            'src/util/str_util.c',
        ]],
        ['test_device_msg_deserialize', [
            'tests/test_device_msg_deserialize.c',
            'src/device_msg.c',
        ]],
        ['test_queue', [
            'tests/test_queue.c',
        ]],
        ['test_strutil', [
            'tests/test_strutil.c',
            'src/util/str_util.c',
        ]],
    ]

    foreach t : tests
        exe = executable(t[0], t[1],
                         include_directories: src_dir,
                         dependencies: dependencies,
                         c_args: ['-DSDL_MAIN_HANDLED', '-DSC_TEST'])
        test(t[0], exe)
    endforeach
endif
