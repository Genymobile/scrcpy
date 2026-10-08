@echo off & setlocal
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { $params = @{ FilePath = '%~dp0scrcpy.exe'; WindowStyle = 'Hidden' }; if ($args.Count -gt 0) { $params['ArgumentList'] = $args }; Start-Process @params }" %*
