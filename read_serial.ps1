$port = New-Object System.IO.Ports.SerialPort 'COM3', 115200, None, 8, One
$port.DtrEnable = $false
$port.RtsEnable = $true
$port.Open()
Start-Sleep -Milliseconds 100
$port.RtsEnable = $false
Start-Sleep -Milliseconds 800
$start = Get-Date
while (((Get-Date) - $start).TotalSeconds -lt 15) {
    if ($port.BytesToRead -gt 0) {
        $data = $port.ReadExisting()
        Write-Host -NoNewline $data
    }
    Start-Sleep -Milliseconds 50
}
$port.Close()
