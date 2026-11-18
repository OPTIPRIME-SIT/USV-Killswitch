Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[System.Windows.Forms.Application]::EnableVisualStyles()

$form = New-Object System.Windows.Forms.Form
$form.Text = 'SiK Button Monitor'
$form.Size = New-Object System.Drawing.Size(720, 520)
$form.MinimumSize = New-Object System.Drawing.Size(600, 430)
$form.StartPosition = 'CenterScreen'
$form.BackColor = [System.Drawing.Color]::FromArgb(18, 22, 30)
$form.ForeColor = [System.Drawing.Color]::White
$form.Font = New-Object System.Drawing.Font('Segoe UI', 11)

$title = New-Object System.Windows.Forms.Label
$title.Text = 'BUTTON RECEIVER'
$title.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 22)
$title.AutoSize = $true
$title.Location = New-Object System.Drawing.Point(24, 20)
$form.Controls.Add($title)

$portPicker = New-Object System.Windows.Forms.ComboBox
$portPicker.DropDownStyle = 'DropDownList'
$portPicker.Location = New-Object System.Drawing.Point(28, 73)
$portPicker.Size = New-Object System.Drawing.Size(135, 32)
$form.Controls.Add($portPicker)

$connectButton = New-Object System.Windows.Forms.Button
$connectButton.Text = 'Connect'
$connectButton.Location = New-Object System.Drawing.Point(175, 71)
$connectButton.Size = New-Object System.Drawing.Size(105, 34)
$form.Controls.Add($connectButton)

$refreshButton = New-Object System.Windows.Forms.Button
$refreshButton.Text = 'Refresh ports'
$refreshButton.Location = New-Object System.Drawing.Point(292, 71)
$refreshButton.Size = New-Object System.Drawing.Size(125, 34)
$form.Controls.Add($refreshButton)

$status = New-Object System.Windows.Forms.Label
$status.Text = '● Disconnected'
$status.ForeColor = [System.Drawing.Color]::FromArgb(255, 120, 120)
$status.AutoSize = $true
$status.Location = New-Object System.Drawing.Point(438, 79)
$form.Controls.Add($status)

$leftPanel = New-Object System.Windows.Forms.Panel
$leftPanel.Location = New-Object System.Drawing.Point(28, 130)
$leftPanel.Size = New-Object System.Drawing.Size(310, 145)
$leftPanel.BackColor = [System.Drawing.Color]::FromArgb(42, 49, 62)
$form.Controls.Add($leftPanel)

$leftLabel = New-Object System.Windows.Forms.Label
$leftLabel.Text = 'START'
$leftLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 30)
$leftLabel.AutoSize = $true
$leftLabel.Location = New-Object System.Drawing.Point(65, 42)
$leftPanel.Controls.Add($leftLabel)

$rightPanel = New-Object System.Windows.Forms.Panel
$rightPanel.Location = New-Object System.Drawing.Point(354, 130)
$rightPanel.Size = New-Object System.Drawing.Size(310, 145)
$rightPanel.BackColor = [System.Drawing.Color]::FromArgb(42, 49, 62)
$form.Controls.Add($rightPanel)

$rightLabel = New-Object System.Windows.Forms.Label
$rightLabel.Text = 'STOP'
$rightLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 30)
$rightLabel.AutoSize = $true
$rightLabel.Location = New-Object System.Drawing.Point(88, 42)
$rightPanel.Controls.Add($rightLabel)

$lastEvent = New-Object System.Windows.Forms.Label
$lastEvent.Text = 'Relay state: waiting for receiver'
$lastEvent.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 16)
$lastEvent.AutoSize = $true
$lastEvent.Location = New-Object System.Drawing.Point(28, 296)
$form.Controls.Add($lastEvent)

$eventLog = New-Object System.Windows.Forms.ListBox
$eventLog.Location = New-Object System.Drawing.Point(28, 338)
$eventLog.Size = New-Object System.Drawing.Size(636, 105)
$eventLog.BackColor = [System.Drawing.Color]::FromArgb(26, 31, 41)
$eventLog.ForeColor = [System.Drawing.Color]::Gainsboro
$eventLog.BorderStyle = 'FixedSingle'
$form.Controls.Add($eventLog)

$serial = New-Object System.IO.Ports.SerialPort
$serial.BaudRate = 115200
$serial.NewLine = "`n"
$serial.ReadTimeout = 20
$receiveBuffer = ''
$highlightUntil = [datetime]::MinValue
$normalColour = [System.Drawing.Color]::FromArgb(42, 49, 62)
$activeColour = [System.Drawing.Color]::FromArgb(25, 150, 95)

function Refresh-Ports {
    $selected = $portPicker.SelectedItem
    $portPicker.Items.Clear()
    foreach ($port in [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object) {
        [void]$portPicker.Items.Add($port)
    }
    if ($selected -and $portPicker.Items.Contains($selected)) {
        $portPicker.SelectedItem = $selected
    } elseif ($portPicker.Items.Count -gt 0) {
        $portPicker.SelectedIndex = $portPicker.Items.Count - 1
    }
}

function Show-Button([string]$buttonName) {
    $leftPanel.BackColor = $normalColour
    $rightPanel.BackColor = $normalColour
    if ($buttonName -eq 'LEFT') { $leftPanel.BackColor = $activeColour }
    if ($buttonName -eq 'RIGHT') { $rightPanel.BackColor = $activeColour }
    $lastEvent.Text = "Last received: $buttonName"
    [void]$eventLog.Items.Insert(0, "$(Get-Date -Format 'HH:mm:ss.fff')  $buttonName")
    while ($eventLog.Items.Count -gt 25) { $eventLog.Items.RemoveAt(25) }
    $highlightUntil = (Get-Date).AddMilliseconds(650)
}

function Show-RelayState([string]$relayState) {
    $leftPanel.BackColor = $normalColour
    $rightPanel.BackColor = $normalColour
    if ($relayState -eq 'STARTED') {
        $leftPanel.BackColor = $activeColour
        $lastEvent.Text = 'Relay state: STARTED'
    } else {
        $rightPanel.BackColor = [System.Drawing.Color]::FromArgb(190, 65, 65)
        $lastEvent.Text = 'Relay state: STOPPED / CUT OFF'
    }
    [void]$eventLog.Items.Insert(0, "$(Get-Date -Format 'HH:mm:ss.fff')  RELAY $relayState")
    while ($eventLog.Items.Count -gt 25) { $eventLog.Items.RemoveAt(25) }
    $highlightUntil = (Get-Date).AddMilliseconds(1200)
}

$refreshButton.Add_Click({ Refresh-Ports })

$connectButton.Add_Click({
    try {
        if ($serial.IsOpen) {
            $serial.Close()
            $connectButton.Text = 'Connect'
            $status.Text = '● Disconnected'
            $status.ForeColor = [System.Drawing.Color]::FromArgb(255, 120, 120)
            return
        }
        if (-not $portPicker.SelectedItem) { throw 'Select a COM port first.' }
        $serial.PortName = [string]$portPicker.SelectedItem
        $serial.Open()
        $receiveBuffer = ''
        $connectButton.Text = 'Disconnect'
        $status.Text = "● Connected to $($serial.PortName)"
        $status.ForeColor = [System.Drawing.Color]::FromArgb(100, 230, 155)
    } catch {
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Connection error') | Out-Null
    }
})

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 40
$timer.Add_Tick({
    if ($serial.IsOpen) {
        try {
            $receiveBuffer += $serial.ReadExisting()
            while ($receiveBuffer.Contains("`n")) {
                $split = $receiveBuffer.IndexOf("`n")
                $line = $receiveBuffer.Substring(0, $split).Trim()
                $receiveBuffer = $receiveBuffer.Substring($split + 1)
                if ($line -match '^RECEIVED BUTTON (LEFT|RIGHT)$') {
                    Show-Button $Matches[1]
                }
                if ($line -match '^RELAY (STARTED|STOPPED)$') {
                    Show-RelayState $Matches[1]
                }
                if ($line -match '^STATUS RELAY (STARTED|STOPPED)$') {
                    $leftPanel.BackColor = $normalColour
                    $rightPanel.BackColor = $normalColour
                    if ($Matches[1] -eq 'STARTED') {
                        $leftPanel.BackColor = $activeColour
                        $lastEvent.Text = 'Relay state: STARTED'
                    } else {
                        $rightPanel.BackColor = [System.Drawing.Color]::FromArgb(190, 65, 65)
                        $lastEvent.Text = 'Relay state: STOPPED / CUT OFF'
                    }
                }
                if ($line -match '^RADIO RAW: (.*)$') {
                    [void]$eventLog.Items.Insert(0, "$(Get-Date -Format 'HH:mm:ss.fff')  RAW: $($Matches[1])")
                    while ($eventLog.Items.Count -gt 25) { $eventLog.Items.RemoveAt(25) }
                }
                if ($line -eq 'RADIO PARSE ERROR') {
                    [void]$eventLog.Items.Insert(0, "$(Get-Date -Format 'HH:mm:ss.fff')  Packet format error")
                }
            }
        } catch {
            $serial.Close()
            $connectButton.Text = 'Connect'
            $status.Text = '● Connection lost'
            $status.ForeColor = [System.Drawing.Color]::FromArgb(255, 120, 120)
        }
    }
})

$form.Add_FormClosing({
    $timer.Stop()
    if ($serial.IsOpen) { $serial.Close() }
})

Refresh-Ports
$timer.Start()
[void]$form.ShowDialog()
