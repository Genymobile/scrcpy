project('scrcpy-app', 'c')

src = [
    'src/scrcpy.c',
    'src/command.c',
    'src/decoder.c',
    'src/frames.c',
    'src/lockutil.c',
    'src/netutil.c',
    'src/screen.c',
    'src/strutil.c',
]

if host_machine.system() == 'windows'
    src += [ 'src/win/command.c' ]
else
    src += [ 'src/unix/command.c' ]
endif

dependencies = [
    dependency('libavformat'),
    dependency('libavcodec'),
    dependency('libavutil'),
    dependency('sdl2'),
    dependency('SDL2_net'),
]

executable('scrcpy', src, dependencies: dependencies)
