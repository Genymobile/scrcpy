Dim sh, fso, scriptDir, cmd, arg
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
cmd = "cmd /c cd /d """ & scriptDir & """ && scrcpy.exe"

For Each arg In WScript.Arguments
    cmd = cmd & " """ & Replace(arg, """", """""") & """"
Next

sh.Run cmd, 0, False
