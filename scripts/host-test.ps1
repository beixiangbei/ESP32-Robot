$ErrorActionPreference = "Stop"

$scriptPath = Join-Path $PSScriptRoot "host-test.sh"
$wslScriptPath = (wsl wslpath -a ($scriptPath -replace '\\', '/')).Trim()

if (-not $wslScriptPath) {
    throw "Unable to resolve the host test script path in WSL."
}

wsl sh $wslScriptPath @args
if ($LASTEXITCODE -ne 0) {
    throw "Host tests failed with exit code $LASTEXITCODE."
}

