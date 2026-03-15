@echo off
setlocal ENABLEDELAYEDEXPANSION

:: =============
:: PAIR FIRST
:: =============
:: If the connection fails, pair this computer to the android device first:
:: On the android device:
:: Go to wireless debugging, and then tap on "pair", there will be an IP:port
:: and pairing-code shown.
:: On the computer, in a terminal, run the following command:
:: adb pair IP:port pairing-code.
:: After pairing, run this script, and it should connect automatically.
:: Without pairing, the android device will reject connection attempts.
:: Update the config.ini either via the prompts or manually with the IP and
:: port values to allow the connection to work correctly.


:: ============================
:: LOAD CONFIG (config.ini)
:: ============================
if not exist "config.ini" (
    >config.ini (
        echo DEVICE_IP=192.168.0.1
        echo DEVICE_PORT=5555
        echo MAX_ATTEMPTS=5
    )
)

for /f "usebackq tokens=1,2 delims==" %%A in ("config.ini") do (
    set "%%A=%%B"
)

echo Loaded configuration:
echo   IP: %DEVICE_IP%
echo   Port: %DEVICE_PORT%
echo   Max attempts: %MAX_ATTEMPTS%
echo.

:: =========
:: IP PROMPT
:: =========
set /p USER_IP=Enter device IP Address (default %DEVICE_IP%):
if not "%USER_IP%"=="" (
    set DEVICE_IP=%USER_IP%
)

echo Using IP Address: %DEVICE_IP%
echo.

:: ===========
:: PORT PROMPT
:: ===========
set /p USER_PORT=Enter device port (default %DEVICE_PORT%):
if not "%USER_PORT%"=="" (
    set DEVICE_PORT=%USER_PORT%
)

echo Using port: %DEVICE_PORT%
echo.

:: ===================
:: MAX_ATTEMPTS PROMPT
:: ===================
set /p USER_MAX_ATTEMPTS=Enter max retries (default %MAX_ATTEMPTS%):
if not "%USER_MAX_ATTEMPTS%"=="" (
    set MAX_ATTEMPTS=%USER_MAX_ATTEMPTS%
)

echo Using max retries: %MAX_ATTEMPTS%
echo.

:: ============================
:: SAVE UPDATED CONFIG
:: ============================
>config.ini (
    echo DEVICE_IP=%DEVICE_IP%
    echo DEVICE_PORT=%DEVICE_PORT%
    echo MAX_ATTEMPTS=%MAX_ATTEMPTS%
)

:: ================
:: MAIN ENTRY POINT
:: ================
SET ATTEMPT=0
echo Starting ADB connection sequence...
echo Target Device: %DEVICE_IP%:%DEVICE_PORT%
echo.

goto :MAIN


:: ==================
:: FUNCTIONS / LABELS
:: ==================

:MAIN
call :WAIT_CONNECT
if !ERRORLEVEL! NEQ 0 (
    echo.
    echo Connection failed after %MAX_ATTEMPTS% attempts.
    goto :END
)

call :START_SCRCPY
goto :END


:WAIT_CONNECT
echo Attempting connection...

:RETRY_LOOP
set /a ATTEMPT+=1
echo Attempt !ATTEMPT! of %MAX_ATTEMPTS%...

adb connect %DEVICE_IP%:%DEVICE_PORT% > NUL

:: Check if device appears in adb devices list
adb devices | findstr /C:"%DEVICE_IP%:%DEVICE_PORT%" > NUL
if !ERRORLEVEL! NEQ 0 (
    echo Device not connected. Retrying...
    goto :RETRY_DELAY
)

:: Check if device is unauthorised in adb devices list
adb devices | findstr /C:"%DEVICE_IP%:%DEVICE_PORT%" | findstr /I "unauthorized" > NUL
if !ERRORLEVEL! EQU 0 (
    echo Device detected but NOT authorized.
    echo Please accept the prompt on the phone.
    goto :RETRY_DELAY
)

:: SUCCESS If we reach here, device is authorised
echo Device connected and authorized.
exit /b 0


:RETRY_DELAY
:: If not connected, check if we exceeded max attempts
if !ATTEMPT! GEQ %MAX_ATTEMPTS% (
    exit /b 1
)
timeout /t 3
goto :RETRY_LOOP


:START_SCRCPY
echo Launching scrcpy...
:: scrcpy.exe --tcpip=%DEVICE_IP%:%DEVICE_PORT% --pause-on-exit=if-error --kill-adb-on-close %* REM this kills adb on exit
scrcpy.exe --tcpip=%DEVICE_IP%:%DEVICE_PORT% --pause-on-exit=if-error %*
exit /b


:: ================
:: CLEANUP AND EXIT
:: ================

:END
echo.
echo Done.
echo Press any key to close...
pause >NUL
endlocal
exit /b
