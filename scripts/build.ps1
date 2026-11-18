$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'build'
$releaseRoot = Join-Path $repoRoot 'release'
$board = 'rp2040:rp2040:rpipico'
$indexUrl = 'https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json'

if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    throw 'arduino-cli is not installed or is not available on PATH.'
}

New-Item -ItemType Directory -Force -Path $buildRoot, $releaseRoot | Out-Null

arduino-cli core update-index --additional-urls $indexUrl
arduino-cli core install 'rp2040:rp2040@6.0.0' --additional-urls $indexUrl
arduino-cli lib install 'Adafruit GFX Library@1.12.6'
arduino-cli lib install 'Adafruit ST7735 and ST7789 Library@1.11.0'

$transmitterBuild = Join-Path $buildRoot 'transmitter'
$receiverBuild = Join-Path $buildRoot 'receiver'
New-Item -ItemType Directory -Force -Path $transmitterBuild, $receiverBuild | Out-Null

arduino-cli compile --fqbn $board --output-dir $transmitterBuild (Join-Path $repoRoot 'firmware\transmitter')
arduino-cli compile --fqbn $board --output-dir $receiverBuild (Join-Path $repoRoot 'firmware\receiver')

Copy-Item -Force (Join-Path $transmitterBuild 'transmitter.ino.uf2') (Join-Path $releaseRoot 'transmitter.uf2')
Copy-Item -Force (Join-Path $receiverBuild 'receiver.ino.uf2') (Join-Path $releaseRoot 'receiver.uf2')

Write-Host "Built UF2 files in $releaseRoot" -ForegroundColor Green
