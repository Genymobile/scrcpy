Dim sh, fso, scriptDir, cmd, arg
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
cmd = "cmd /c cd /d """ & scriptDir & """ && scrcpy.exe --no-vd-system-decorations --new-display=602x370/160 --flex-display --render-fit=unscaled --audio-buffer=200 --keep-active --start-app=app.lawnchair"

For Each arg In WScript.Arguments
    cmd = cmd & " """ & Replace(arg, """", """""") & """"
Next

sh.Run cmd, 0, False
