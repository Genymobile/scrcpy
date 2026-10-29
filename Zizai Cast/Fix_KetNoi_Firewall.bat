@echo off
chcp 65001 >nul
title SỬA LỖI KẾT NỐI AIRPLAY CHO ZIZAI CAST
echo Đang mở quyền Quản trị viên (Administrator) để cho phép Zizai Cast qua Tường lửa...
powershell -Command "Start-Process cmd -Verb RunAs -ArgumentList '/c netsh advfirewall firewall delete rule name=\"自在投影.exe\" & netsh advfirewall firewall add rule name=\"Zizai Cast (Allow All)\" dir=in action=allow program=\"C:\Users\TRANTHAJCH\Desktop\Zizai Cast\程式\自在投影.exe\" enable=yes profile=any & netsh advfirewall firewall add rule name=\"AirPlay 7000\" dir=in action=allow protocol=TCP localport=7000 profile=any & netsh advfirewall firewall add rule name=\"AirPlay 5353\" dir=in action=allow protocol=UDP localport=5353 profile=any & echo. & echo ============================================== & echo DA SUA XONG TUONG LUA THANH CONG! & echo ============================================== & pause'"
