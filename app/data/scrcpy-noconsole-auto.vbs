Set WshShell = CreateObject("Wscript.Shell")

Do
    ' Wait for phone
    WshShell.Run "cmd /c adb wait-for-device && scrcpy.exe", 0, True

    ' When scrcpy closes, loop back and wait again
    WScript.Sleep 2000
Loop
