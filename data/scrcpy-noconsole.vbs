strCommand = "cmd /c scrcpy.exe"

For Each Arg In WScript.Arguments
    strCommand = strCommand & " """ & Arg & """"
Next

CreateObject("Wscript.Shell").Run strCommand, 0, false
