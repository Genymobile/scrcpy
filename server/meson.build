# It may be useful to use a prebuilt server, so that no Android SDK is required
# to build. If the 'prebuilt_server' option is set, just copy the file as is.
prebuilt_server = get_option('prebuilt_server')
if prebuilt_server == ''
    custom_target('scrcpy-server',
                  build_always: true,  # gradle is responsible for tracking source changes
                  output: 'scrcpy-server',
                  command: [find_program('./scripts/build-wrapper.sh'), meson.current_source_dir(), '@OUTPUT@', get_option('buildtype')],
                  console: true,
                  install: true,
                  install_dir: 'share/scrcpy')
else
    if not prebuilt_server.startswith('/')
        # relative path needs some trick
        prebuilt_server = meson.source_root() + '/' + prebuilt_server
    endif
    custom_target('scrcpy-server-prebuilt',
                  input: prebuilt_server,
                  output: 'scrcpy-server',
                  command: ['cp', '@INPUT@', '@OUTPUT@'],
                  install: true,
                  install_dir: 'share/scrcpy')
endif
