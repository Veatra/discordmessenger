# Windows 8.1 / PowerShell 4 compatible. Run beside DiscordMessenger.exe.
# This report is independent of the application's optional runtime log.
$ErrorActionPreference = 'Stop'
$folder = Split-Path -Parent $MyInvocation.MyCommand.Path
$report = Join-Path $folder 'DiscordMessenger-loader-report.txt'
function Report($line) { Add-Content -LiteralPath $report -Value $line -Encoding UTF8 }
function PeInfo($path) {
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 64 -or $bytes[0] -ne 77 -or $bytes[1] -ne 90) { throw "Not a Windows PE file: $path" }
    $offset = [BitConverter]::ToInt32($bytes, 60)
    if ($offset -lt 0 -or $offset + 96 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,$offset) -ne 17744) { throw "Invalid PE header: $path" }
    $machine = [BitConverter]::ToUInt16($bytes,$offset+4)
    $subMajor = [BitConverter]::ToUInt16($bytes,$offset+72)
    $subMinor = [BitConverter]::ToUInt16($bytes,$offset+74)
    return ('Machine=0x{0:X4} (014C=x86, 8664=x64); loader subsystem={1}.{2}' -f $machine,$subMajor,$subMinor)
}
try {
    Set-Content -LiteralPath $report -Value ('Loader diagnostic report: ' + (Get-Date -Format o)) -Encoding UTF8
    Report ('OS: ' + [Environment]::OSVersion.VersionString)
    Report ('64-bit OS: ' + [Environment]::Is64BitOperatingSystem)
    Report ('Package folder: ' + $folder)
    $files = @('DiscordMessenger.exe','libcrypto-4.dll','libssl-4.dll','libwinpthread-1.dll')
    foreach ($name in $files) {
        $path = Join-Path $folder $name
        if (Test-Path -LiteralPath $path) {
            Report ($name + ': ' + (PeInfo $path))
            Report ('SHA256: ' + (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)
        } else { Report ('MISSING: ' + $name) }
    }
    Report 'Launching application; close it to finish the report.'
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = Join-Path $folder 'DiscordMessenger.exe'
    $process.StartInfo.WorkingDirectory = $folder
    $process.StartInfo.UseShellExecute = $false
    [void]$process.Start()
    $process.WaitForExit()
    Report ('Process exit code (decimal): ' + $process.ExitCode)
    if ($null -ne $process.ExitCode) {
        $unsigned = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$process.ExitCode),0)
        Report ('Process exit code (hex): 0x{0:X8}' -f $unsigned)
    }
} catch {
    Report ('Launcher error: ' + $_.Exception.Message)
    Report ('Exception type: ' + $_.Exception.GetType().FullName)
    if ($_.Exception.NativeErrorCode) { Report ('Win32 error: ' + $_.Exception.NativeErrorCode) }
}
Write-Host ('Report saved to ' + $report)
