Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = "scrcpy Wireless"
$form.Size = New-Object System.Drawing.Size(430, 430)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.BackColor = [System.Drawing.Color]::FromArgb(18, 18, 20)
$form.ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
$form.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$form.Padding = New-Object System.Windows.Forms.Padding(18)

$title = New-Object System.Windows.Forms.Label
$title.Text = "scrcpy Wireless"
$title.Location = New-Object System.Drawing.Point(20, 15)
$title.Size = New-Object System.Drawing.Size(220, 28)
$title.Font = New-Object System.Drawing.Font("Segoe UI", 13, [System.Drawing.FontStyle]::Bold)
$title.ForeColor = [System.Drawing.Color]::FromArgb(245, 245, 245)
$form.Controls.Add($title)

$subtitle = New-Object System.Windows.Forms.Label
$subtitle.Text = "Enter the device details, then hit Submit."
$subtitle.Location = New-Object System.Drawing.Point(22, 42)
$subtitle.Size = New-Object System.Drawing.Size(220, 20)
$subtitle.Font = New-Object System.Drawing.Font("Segoe UI", 8)
$subtitle.ForeColor = [System.Drawing.Color]::FromArgb(150, 150, 160)
$form.Controls.Add($subtitle)

function New-LabeledField {
    param(
        [string]$LabelText,
        [string]$Placeholder,
        [int]$Top
    )

    $lbl = New-Object System.Windows.Forms.Label
    $lbl.Text = $LabelText
    $lbl.Location = New-Object System.Drawing.Point(22, $Top)
    $lbl.Size = New-Object System.Drawing.Size(360, 18)
    $lbl.ForeColor = [System.Drawing.Color]::FromArgb(180, 180, 190)
    $lbl.Font = New-Object System.Drawing.Font("Segoe UI", 9)
    $form.Controls.Add($lbl)

    $txt = New-Object System.Windows.Forms.TextBox
    $txt.Location = New-Object System.Drawing.Point(22, ($Top + 22))
    $txt.Size = New-Object System.Drawing.Size(370, 30)
    $txt.BackColor = [System.Drawing.Color]::FromArgb(28, 28, 32)
    $txt.ForeColor = [System.Drawing.Color]::FromArgb(80, 80, 88)
    $txt.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
    $txt.Font = New-Object System.Drawing.Font("Consolas", 11)
    $txt.Tag = $Placeholder
    $txt.Text = $Placeholder

    $txt.Add_Enter({
        if ($this.Text -eq $this.Tag) {
            $this.Text = ""
            $this.ForeColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
        }
    })

    $txt.Add_Leave({
        if ([string]::IsNullOrWhiteSpace($this.Text)) {
            $this.Text = $this.Tag
            $this.ForeColor = [System.Drawing.Color]::FromArgb(80, 80, 88)
        }
    })

    $form.Controls.Add($txt)
    return $txt
}

$txIP          = New-LabeledField -LabelText "Device IP"       -Placeholder "192.168.x.x" -Top 72
$txPairPort    = New-LabeledField -LabelText "Pair port"       -Placeholder "35XXX"      -Top 132
$txPairCode    = New-LabeledField -LabelText "Pairing code"    -Placeholder "123456"     -Top 192
$txConnectPort = New-LabeledField -LabelText "Connect port"    -Placeholder "5555"       -Top 252

$status = New-Object System.Windows.Forms.Label
$status.Text = ""
$status.Location = New-Object System.Drawing.Point(22, 314)
$status.Size = New-Object System.Drawing.Size(370, 18)
$status.Font = New-Object System.Drawing.Font("Segoe UI", 8)
$status.ForeColor = [System.Drawing.Color]::FromArgb(150, 150, 160)
$form.Controls.Add($status)

$btn = New-Object System.Windows.Forms.Button
$btn.Text = "Submit"
$btn.Location = New-Object System.Drawing.Point(22, 334)
$btn.Size = New-Object System.Drawing.Size(370, 30)
$btn.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
$btn.ForeColor = [System.Drawing.Color]::FromArgb(12, 12, 13)
$btn.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$btn.FlatAppearance.BorderSize = 0
$btn.Font = New-Object System.Drawing.Font("Segoe UI", 9, [System.Drawing.FontStyle]::Bold)
$btn.Cursor = [System.Windows.Forms.Cursors]::Hand

function Launch-ScrcpyVbs {
    $vbsPath = Join-Path $PSScriptRoot "scrcpy-nocmd.vbs"
    if (Test-Path $vbsPath) {
        Start-Process -FilePath $vbsPath -WorkingDirectory $PSScriptRoot
    } else {
        [System.Windows.Forms.MessageBox]::Show(
            "scrcpy-nocmd.vbs not found in:`n$PSScriptRoot",
            "scrcpy Wireless Connect",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error
        ) | Out-Null
        return
    }
}

$btnKill = New-Object System.Windows.Forms.Button
$btnKill.Text = "Kill ADB"
$btnKill.Location = New-Object System.Drawing.Point(250, 16)
$btnKill.Size = New-Object System.Drawing.Size(72, 26)
$btnKill.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
$btnKill.ForeColor = [System.Drawing.Color]::FromArgb(12, 12, 13)
$btnKill.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$btnKill.FlatAppearance.BorderSize = 0
$btnKill.Font = New-Object System.Drawing.Font("Segoe UI", 8, [System.Drawing.FontStyle]::Bold)
$btnKill.Cursor = [System.Windows.Forms.Cursors]::Hand
$btnKill.Add_Click({
    $status.Text = "Stopping adb server..."
    [System.Windows.Forms.Application]::DoEvents()
    $out = & cmd /c "adb kill-server 2>&1"
    $status.Text = "ADB server stopped."
    [System.Windows.Forms.MessageBox]::Show(
        "adb kill-server finished.`n$out",
        "scrcpy Wireless Connect",
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Information
    ) | Out-Null
})

$btnLaunch = New-Object System.Windows.Forms.Button
$btnLaunch.Text = "Launch"
$btnLaunch.Location = New-Object System.Drawing.Point(328, 16)
$btnLaunch.Size = New-Object System.Drawing.Size(72, 26)
$btnLaunch.BackColor = [System.Drawing.Color]::FromArgb(240, 240, 240)
$btnLaunch.ForeColor = [System.Drawing.Color]::FromArgb(12, 12, 13)
$btnLaunch.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$btnLaunch.FlatAppearance.BorderSize = 0
$btnLaunch.Font = New-Object System.Drawing.Font("Segoe UI", 8, [System.Drawing.FontStyle]::Bold)
$btnLaunch.Cursor = [System.Windows.Forms.Cursors]::Hand
$btnLaunch.Add_Click({
    $status.Text = "Launching scrcpy..."
    [System.Windows.Forms.Application]::DoEvents()

    $vbsPath = Join-Path $PSScriptRoot "scrcpy-nocmd.vbs"
    if (Test-Path $vbsPath) {
        Start-Process -FilePath $vbsPath -WorkingDirectory $PSScriptRoot
    } else {
        Start-Process "scrcpy.exe" -ArgumentList "--no-vd-system-decorations --new-display=1920x1080/160 --flex-display --render-fit=unscaled --audio-buffer=200 --keep-active --start-app=app.lawnchair"
    }

    $form.Close()
})

$form.Controls.Add($btnKill)
$form.Controls.Add($btnLaunch)

function Get-FieldValue {
    param([System.Windows.Forms.TextBox]$Ctrl)
    if ($Ctrl.Text -eq $Ctrl.Tag) { return "" }
    return $Ctrl.Text.Trim()
}

$btn.Add_Click({
    $status.Text = ""

    $ip          = Get-FieldValue $txIP
    $pairPort    = Get-FieldValue $txPairPort
    $pairCode    = Get-FieldValue $txPairCode
    $connectPort = Get-FieldValue $txConnectPort

    if (-not $ip -or -not $pairPort -or -not $pairCode -or -not $connectPort) {
        [System.Windows.Forms.MessageBox]::Show(
            "Fill all four fields.",
            "scrcpy Wireless Connect",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Warning
        ) | Out-Null
        return
    }

    $status.Text = "Pairing device..."
    [System.Windows.Forms.Application]::DoEvents()

    $pairOut = & cmd /c "adb pair ${ip}:${pairPort} ${pairCode} 2>&1"
    if ($pairOut -notmatch "successfully|paired") {
        [System.Windows.Forms.MessageBox]::Show(
            "adb pair failed:`n$pairOut",
            "scrcpy Wireless Connect",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error
        ) | Out-Null
        return
    }

    $status.Text = "Connecting device..."
    [System.Windows.Forms.Application]::DoEvents()

    $connOut = & cmd /c "adb connect ${ip}:${connectPort} 2>&1"
    if ($connOut -notmatch "connected") {
        [System.Windows.Forms.MessageBox]::Show(
            "adb connect failed:`n$connOut",
            "scrcpy Wireless Connect",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error
        ) | Out-Null
        return
    }

    $status.Text = "Launching scrcpy..."
    [System.Windows.Forms.Application]::DoEvents()

    Launch-ScrcpyVbs
    $form.Close()
})

$form.AcceptButton = $btn
$form.Controls.Add($btn)
$form.ShowDialog() | Out-Null
