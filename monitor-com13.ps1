$port = New-Object System.IO.Ports.SerialPort 'COM13', 115200, 'None', 8, 'One'
$port.DtrEnable = $true
$port.ReadTimeout = 500

try {
    $port.Open()
    Write-Host 'Monitoring receiver COM13. Press Ctrl+C to stop.' -ForegroundColor Cyan
    while ($true) {
        try {
            $line = $port.ReadLine().Trim()
            if ($line -eq 'GPIO10=HIGH') {
                Write-Host "$line  RESET/START" -ForegroundColor Green
            } elseif ($line -eq 'GPIO10=LOW') {
                Write-Host "$line  KILL/STOP" -ForegroundColor Red
            } else {
                Write-Host $line
            }
        } catch [System.TimeoutException] {
        }
    }
} finally {
    if ($port.IsOpen) {
        $port.Close()
    }
}
