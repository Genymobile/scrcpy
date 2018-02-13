project('scrcpy', 'c', subproject_dir: '.')

subproject('app')
subproject('server')

run_target('run', command: ['scripts/run-scrcpy.sh'])
