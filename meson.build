project('scrcpy', 'c',
        version: '1.10',
        meson_version: '>= 0.37',
        default_options: 'c_std=c11')

if get_option('compile_app')
    subdir('app')
endif

if get_option('compile_server')
    subdir('server')
endif

run_target('run', command: ['scripts/run-scrcpy.sh'])
