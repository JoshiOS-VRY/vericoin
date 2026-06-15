# Binary Chain v3 (DACE) - bootstrap a binarytest network from scratch (PowerShell).
# Equivalent to scripts/bootstrap.sh for Windows users.

$ErrorActionPreference = "Stop"

Push-Location -Path (Join-Path $PSScriptRoot "..\docker")
try {
    Write-Host "==> Building images" -ForegroundColor Cyan
    docker compose build

    Write-Host "==> Starting vericoind + veriumd + observer (binarytest mode)" -ForegroundColor Cyan
    docker compose up -d --remove-orphans

    Write-Host "==> Waiting for daemons to be healthy..." -ForegroundColor Cyan
    $deadline = (Get-Date).AddSeconds(180)
    while ($true) {
        $vrc = (docker inspect -f '{{.State.Health.Status}}' bc-vericoind 2>$null)
        $vrm = (docker inspect -f '{{.State.Health.Status}}' bc-veriumd 2>$null)
        if (-not $vrc) { $vrc = "starting" }
        if (-not $vrm) { $vrm = "starting" }
        if ($vrc -eq "healthy" -and $vrm -eq "healthy") { break }
        if ((Get-Date) -gt $deadline) {
            Write-Error "Daemons not healthy after 180s. Check 'docker compose logs'."
        }
        Write-Host "    vrc=$vrc  vrm=$vrm"
        Start-Sleep -Seconds 5
    }

    Write-Host "==> Both daemons healthy. Initial status:" -ForegroundColor Green
    Write-Host "--- VRC binarychain_status ---"
    docker exec bc-vericoind vericoin-cli -binarytest -vericoin binarychain_status
    Write-Host "--- VRM binarychain_status ---"
    docker exec bc-veriumd verium-cli -binarytest -verium binarychain_status

    Write-Host ""
    Write-Host "==> Observer metrics at http://localhost:9101/metrics" -ForegroundColor Cyan
    Write-Host "==> Next: run scripts/advance-to-activation.ps1" -ForegroundColor Cyan
}
finally {
    Pop-Location
}
