project('scrcpy-app', 'c')

src = [
    'src/main.c',
    'src/command.c',
    'src/controlevent.c',
    'src/controller.c',
    'src/convert.c',
    'src/decoder.c',
    'src/frames.c',
    'src/lockutil.c',
    'src/netutil.c',
    'src/scrcpy.c',
    'src/screen.c',
    'src/server.c',
    'src/strutil.c',
    'src/tinyxpm.c',
]

if host_machine.system() == 'windows'
    src += [ 'src/sys/win/command.c' ]
else
    src += [ 'src/sys/unix/command.c' ]
endif

dependencies = [
    dependency('libavformat'),
    dependency('libavcodec'),
    dependency('libavutil'),
    dependency('sdl2'),
    dependency('SDL2_net'),
]

conf = configuration_data()

# the version, updated on release
conf.set_quoted('SCRCPY_VERSION', '0.1')

# the default client TCP port for the "adb reverse" tunnel
# overridden by option --port
conf.set('DEFAULT_LOCAL_PORT', '27183')

# the default max video size for both dimensions, in pixels
# overridden by option --max-size
conf.set('DEFAULT_MAX_SIZE', '0')  # 0: unlimited

# the default video bitrate, in bits/second
# overridden by option --bit-rate
conf.set('DEFAULT_BIT_RATE', '4000000')  # 4Mbps

# whether the app should always display the most recent available frame, even
# if the previous one has not been displayed
# SKIP_FRAMES improves latency at the cost of framerate
conf.set('SKIP_FRAMES', true)

configure_file(configuration: conf, output: 'config.h')

executable('scrcpy', src, dependencies: dependencies)


### TESTS

tests = [
    ['test_control_event_queue', ['tests/test_control_event_queue.c', 'src/controlevent.c']],
    ['test_control_event_serialize', ['tests/test_control_event_serialize.c', 'src/controlevent.c']],
    ['test_strutil', ['tests/test_strutil.c', 'src/strutil.c']],
]

src_dir = include_directories('src')

foreach t : tests
    exe = executable(t[0], t[1], include_directories: src_dir, dependencies: dependencies)
    test(t[0], exe)
endforeach
