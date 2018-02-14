project('scrcpy', 'c', meson_version: '>= 0.37')

if get_option('build_app')
    subdir('app')
endif

if get_option('build_server')
    subdir('server')
endif

run_target('run', command: ['scripts/run-scrcpy.sh'])
