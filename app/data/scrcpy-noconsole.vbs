Set fso = CreateObject("Scripting.FileSystemObject")

' Run the scrcpy.exe located next to this script, so that it also works when
' the script is started from another directory (shortcut, pinned taskbar
' icon, other script...)
strExe = fso.BuildPath(fso.GetParentFolderName(WScript.ScriptFullName), "scrcpy.exe")
If Not fso.FileExists(strExe) Then
    strExe = "scrcpy.exe"
End If

' The whole command is enclosed in quotes, because cmd /c removes the first
' and the last quotes
strCommand = "cmd /c """"" & strExe & """"

For Each Arg In WScript.Arguments
    strCommand = strCommand & " """ & replace(Arg, """", """""""""") & """"
Next

strCommand = strCommand & """"

CreateObject("Wscript.Shell").Run strCommand, 0, false
