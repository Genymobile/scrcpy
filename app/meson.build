project('scrcpy-app', 'c')

src = [
    'src/scrcpy.c',
    'src/command.c',
    'src/control.c',
    'src/controlevent.c',
    'src/convert.c',
    'src/decoder.c',
    'src/frames.c',
    'src/lockutil.c',
    'src/netutil.c',
    'src/screen.c',
    'src/strutil.c',
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

executable('scrcpy', src, dependencies: dependencies)


### TESTS

tests = [
    ['test_control_event_queue', ['tests/test_control_event_queue.c', 'src/controlevent.c']],
]

src_dir = include_directories('src')

foreach t : tests
    exe = executable(t[0], t[1], include_directories: src_dir, dependencies: dependencies)
    test(t[0], exe)
endforeach
