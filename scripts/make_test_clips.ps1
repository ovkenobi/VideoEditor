$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$ffmpeg = Join-Path $root "ffmpeg.exe"
$testDir = Join-Path $root "testdata"

New-Item -ItemType Directory -Force $testDir | Out-Null

$first = Join-Path $testDir "a.mp4"
$second = Join-Path $testDir "b.mp4"

Write-Host "Creating $first ..."
& $ffmpeg -y -hide_banner -loglevel error `
    -f lavfi -i "testsrc2=size=640x360:rate=30" `
    -f lavfi -i "sine=frequency=440" `
    -t 3 -c:v libx264 -pix_fmt yuv420p -c:a aac $first

Write-Host "Creating $second ..."
& $ffmpeg -y -hide_banner -loglevel error `
    -f lavfi -i "testsrc2=size=320x240:rate=25" `
    -t 2 -c:v libx264 -pix_fmt yuv420p $second

Write-Host "Test files are in $testDir"
