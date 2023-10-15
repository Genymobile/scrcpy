strProcessName = "scrcpy.exe"
bFound = False

' Check if scrcpy.exe is already running
Set objWMIService = GetObject("winmgmts:\\.\root\cimv2")
Set colProcesses = objWMIService.ExecQuery("Select * from Win32_Process")

For Each objProcess in colProcesses
    If InStr(1, objProcess.Name, strProcessName, vbTextCompare) > 0 Then
        ' scrcpy.exe is already running
        bFound = True
        Exit For
    End If
Next

If bFound Then
    ' Bring scrcpy window to the foreground, unless its minimized
    Set objShell = CreateObject("WScript.Shell")

    ' Activate the window by title
    objShell.AppActivate("2201116PG")

Else
    ' scrcpy.exe is not running, so start it
    strCommand = "cmd /c scrcpy.exe"

    For Each Arg In WScript.Arguments
        strCommand = strCommand & " """ & Replace(Arg, """", """""") & """"
    Next

    CreateObject("WScript.Shell").Run strCommand, 0, False
End If
