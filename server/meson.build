project('scrcpy-server', 'c')  # not really c, but meson expects something

# does not track dependencies, so meson does not guarantees that server is up to date
# call "touch server" or "ninja -t clean server/scrcpy-server.jar" before to rebuild
custom_target('scrcpy-server',
              build_always: false, # do not enable, otherwise "sudo ninja install" will execute gradle!
              input: '.',
              output: 'scrcpy-server.jar',
              command: [find_program('./scripts/build-wrapper.sh'), '@INPUT@', '@OUTPUT@', get_option('buildtype')],
              install: true,
              install_dir: 'share/scrcpy')
