project('scrcpy', 'c',
        version: '4.1',
        meson_version: '>= 0.49',
        default_options: [
            'c_std=c11',
            'warning_level=2',
            'b_ndebug=if-release',
        ])

if host_machine.system() == 'darwin'
    add_languages('objc')
    add_project_arguments('-fobjc-arc', language: 'objc')
endif

add_project_arguments('-Wmissing-prototypes', language: 'c')

if get_option('compile_app')
    subdir('app')
endif

if get_option('compile_server')
    subdir('server')
endif
