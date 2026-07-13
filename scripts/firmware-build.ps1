$ErrorActionPreference = "Stop"

$scriptPath = Join-Path $PSScriptRoot "firmware-build.sh"
$wslScriptPath = (wsl wslpath -a ($scriptPath -replace '\\', '/')).Trim()

if (-not $wslScriptPath) {
    throw "Unable to resolve the firmware build script path in WSL."
}

wsl sh $wslScriptPath @args
if ($LASTEXITCODE -ne 0) {
    throw "Firmware build failed with exit code $LASTEXITCODE."
}

