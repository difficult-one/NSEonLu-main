$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath "$PSScriptRoot\..\..").Path
$outputDir = Join-Path $repo 'build\capture-test'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$fixture = Join-Path $outputDir 'capture.bin'
$csvPath = Join-Path $outputDir 'capture.csv'
& "$PSScriptRoot\test_chassis_power_capture.exe" --dump $fixture
if ($LASTEXITCODE -ne 0) { throw 'Capture fixture failed' }
& "$repo\scripts\decode-chassis-power-capture.ps1" -InputPath $fixture -OutputPath $csvPath
$rows = @(Import-Csv -LiteralPath $csvPath)
if ($rows.Count -ne 150 -or $rows[0].tick_ms -ne '70' -or
    $rows[25].target_rpm_lf -ne '1000' -or $rows[25].budget_w -ne '20') { throw 'Decoded values incorrect' }
$bytes = [IO.File]::ReadAllBytes($fixture)
function AssertRejected([byte[]] $invalid, [string] $reason) {
    [IO.File]::WriteAllBytes($fixture, $invalid)
    $rejected = $false
    try { & "$repo\scripts\decode-chassis-power-capture.ps1" -InputPath $fixture -OutputPath $csvPath }
    catch { $rejected = $true }
    if (!$rejected) { throw ($reason + ' was accepted') }
}
$corrupt = [byte[]]$bytes.Clone()
$corrupt[50] = $corrupt[50] -bxor 1
AssertRejected $corrupt 'Corrupt frame'
AssertRejected ([byte[]]($bytes[0..101] + $bytes[204..($bytes.Length-1)])) 'Missing frame'
AssertRejected ([byte[]]($bytes + $bytes[0..101])) 'Duplicate frame'
AssertRejected ([byte[]]$bytes[0..($bytes.Length-2)]) 'Truncated frame'
$version = [byte[]]$bytes.Clone()
$version[4] = 2
$crc = 0xffff
for ($n = 0; $n -lt 100; ++$n) {
    $crc = $crc -bxor $version[$n]
    for ($bit = 0; $bit -lt 8; ++$bit) {
        if ($crc -band 1) { $crc = ($crc -shr 1) -bxor 0xa001 }
        else { $crc = $crc -shr 1 }
    }
}
$version[100] = [byte]($crc -band 255)
$version[101] = [byte]($crc -shr 8)
AssertRejected $version 'Unsupported version with valid CRC'
Write-Output 'PASS: capture decoder tests'
