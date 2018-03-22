src = [
    'src/main.c',
    'src/command.c',
    'src/controlevent.c',
    'src/controller.c',
    'src/convert.c',
    'src/decoder.c',
    'src/device.c',
    'src/fpscounter.c',
    'src/frames.c',
    'src/inputmanager.c',
    'src/lockutil.c',
    'src/net.c',
    'src/scrcpy.c',
    'src/screen.c',
    'src/server.c',
    'src/strutil.c',
    'src/tinyxpm.c',
]

if get_option('audio_support')
    src += [
        'src/aoa.c',
        'src/audio.c'
    ]
endif

dependencies = [
    dependency('libavformat'),
    dependency('libavcodec'),
    dependency('libavutil'),
    dependency('sdl2'),
]

if get_option('audio_support')
    dependencies += dependency('libusb-1.0')
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
conf.set_quoted('SCRCPY_VERSION', '1.1')

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

# enable audio support (enable audio forwarding with --forward-audio)
conf.set('AUDIO_SUPPORT', get_option('audio_support'))

configure_file(configuration: conf, output: 'config.h')

src_dir = include_directories('src')
executable('scrcpy', src, dependencies: dependencies, include_directories: src_dir, install: true)


### TESTS

tests = [
    ['test_control_event_queue', ['tests/test_control_event_queue.c', 'src/controlevent.c']],
    ['test_control_event_serialize', ['tests/test_control_event_serialize.c', 'src/controlevent.c']],
    ['test_strutil', ['tests/test_strutil.c', 'src/strutil.c']],
]

foreach t : tests
    exe = executable(t[0], t[1], include_directories: src_dir, dependencies: dependencies)
    test(t[0], exe)
endforeach
