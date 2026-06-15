# Build DACE-capable veriumd + vericoind on Windows via WSL.
# PowerShell cannot run ./configure directly - use this script instead.
#
# Usage:
#   cd vericoin
#   .\build-dace.ps1
#
#   - or from repo root -
#   .\vericoin\build-dace.ps1

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vericoinRoot = (Resolve-Path $scriptDir).Path
$repoRoot = Split-Path -Parent $vericoinRoot

function To-WslPath([string]$WinPath) {
    $resolved = (Resolve-Path $WinPath).Path -replace '\\', '/'
    if ($resolved -match '^([A-Za-z]):(.*)$') {
        return "/mnt/$($matches[1].ToLower())$($matches[2])"
    }
    return $resolved
}

$wslVericoin = To-WslPath $vericoinRoot
$wslLog = "~/vericonomy-build/build-dace.log"

Write-Host "==> Building DACE daemons in WSL" -ForegroundColor Cyan
Write-Host "    Windows path: $vericoinRoot"
Write-Host "    WSL path:     $wslVericoin"
Write-Host "    Log file:     $wslLog"
Write-Host ""

# Single-line bash -lc avoids CRLF in here-strings breaking `set -o pipefail`.
$buildCmd = "cd '$wslVericoin' && sed -i 's/\r$//' ./build-dace.sh ./autogen.sh 2>/dev/null || true && chmod +x ./build-dace.sh ./autogen.sh 2>/dev/null || true && ./build-dace.sh"

wsl bash -lc $buildCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Build failed (exit $LASTEXITCODE)." -ForegroundColor Red
    Write-Host ""
    Write-Host "Last 40 lines of the build log:" -ForegroundColor Yellow
    wsl bash -lc "tail -40 '$wslLog' 2>/dev/null || true"
    Write-Host ""
    Write-Host "Common fixes:" -ForegroundColor Yellow
    Write-Host "  - Install WSL build deps (no sudo password needed):"
    Write-Host "      wsl -u root apt-get update"
    Write-Host "      wsl -u root apt-get install -y build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 curl git rsync libevent-dev libboost-all-dev libssl-dev libdb-dev libdb++-dev libminiupnpc-dev libzmq3-dev g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 mingw-w64 mingw-w64-x86-64-dev"
    Write-Host "  - Clear a stale depends tree after script changes:"
    Write-Host "      wsl rm -rf ~/vericonomy-build/depends/work/build/x86_64-w64-mingw32/qt"
    Write-Host "  - Full log: wsl tail -100 $wslLog"
    Write-Host ""
    Write-Host "First build compiles depends/ for MinGW and can take 30+ minutes." -ForegroundColor Yellow
    Write-Host "Sources are copied to ~/vericonomy-build inside WSL (required - /mnt/c breaks autotools)." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "==> Installing DACE sidecars into the wallet" -ForegroundColor Cyan
$walletDir = Join-Path $repoRoot "verium\desktop\verium-app"
Push-Location $walletDir
try {
    npm run fetch:sidecars:dace
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Done. Restart the wallet (npm run tauri:dev) and switch to Binarytest in Settings." -ForegroundColor Green
