@echo off
title scrcpy - Android Screen Mirror
cd /d "%~dp0bin"

echo ========================================================
echo        SCRCPY - PHAN CHIEU MAN HINH ANDROID
echo ========================================================
echo.
echo [1/2] Kiem tra danh sach thiet bi Android da ket noi:
adb.exe devices
echo.

echo [2/2] Dang khoi dong scrcpy...
scrcpy.exe

if errorlevel 1 goto :error
goto :eof

:error
echo.
echo ========================================================
echo [!] KHONG THE KET NOI DEN DIEN THOAI ANDROID
echo.
echo Huong dan xu ly:
echo 1. Cam cap USB noi dien thoai voi may tinh.
echo 2. Tren dien thoai: Vao Cai dat - Tuy chon nha phat trien
echo    - Bat "Go loi USB" (USB Debugging).
echo 3. Mo khoa dien thoai, tich chon "Luon cho phep" va bam
echo    "Cho phep" (Allow USB debugging) tren man hinh.
echo 4. Voi dien thoai Xiaomi / Redmi / POCO: Can bat them
echo    "Go loi USB (Cai dat bao mat)" de dung chuot / ban phim.
echo ========================================================
echo.
pause
