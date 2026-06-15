# Tear down binarytest network and wipe all state (PowerShell).

$ErrorActionPreference = "Stop"
Push-Location -Path (Join-Path $PSScriptRoot "..\docker")
try {
    docker compose down -v --remove-orphans
    Write-Host "==> binarytest network torn down. All volumes wiped." -ForegroundColor Green
}
finally {
    Pop-Location
}
