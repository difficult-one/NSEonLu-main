param([switch]$Fixture)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $buildOutput = & make all 2>&1
    if ($LASTEXITCODE -ne 0) { $buildOutput | Write-Output; exit $LASTEXITCODE }
    if ($Fixture) { & .\test_format.exe --fixture } else { & make test }
    exit $LASTEXITCODE
} finally { Pop-Location }
