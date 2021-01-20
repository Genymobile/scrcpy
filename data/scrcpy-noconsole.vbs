Set Args = WScript.Arguments

strCommand = "cmd /c scrcpy.exe"

If (Args.Count > 0) Then
    strArg = ""
    For Each Arg In Args
        strArg = strArg + " " + Arg
    Next

    strCommand = strCommand + strArg   
End If

CreateObject("Wscript.Shell").Run strCommand, 0, false
