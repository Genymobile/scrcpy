custom_target('scrcpy-server',
              build_always: true,  # gradle is responsible for tracking source changes
              input: '.',
              output: 'scrcpy-server.jar',
              command: [find_program('./scripts/build-wrapper.sh'), '@INPUT@', '@OUTPUT@', get_option('buildtype')],
              install: true,
              install_dir: 'share/scrcpy')
