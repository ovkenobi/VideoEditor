$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "Building..."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build --parallel

$windeployqt = (Get-Command windeployqt -ErrorAction SilentlyContinue).Source
if (-not $windeployqt) {
    $candidate = "C:/msys64/mingw64/bin/windeployqt.exe"
    if (Test-Path $candidate) {
        $windeployqt = $candidate
    }
}
if (-not $windeployqt) {
    throw "windeployqt was not found. Add C:/msys64/mingw64/bin to PATH."
}

$mingwBin = "C:/msys64/mingw64/bin"
$objdump = (Get-Command objdump -ErrorAction SilentlyContinue).Source
if (-not $objdump) {
    $candidate = Join-Path $mingwBin "objdump.exe"
    if (Test-Path $candidate) {
        $objdump = $candidate
    }
}
if (-not $objdump) {
    throw "objdump was not found. Add C:/msys64/mingw64/bin to PATH."
}

$dist = Join-Path $root "dist"
if (Test-Path $dist) {
    Remove-Item $dist -Recurse -Force
}
New-Item -ItemType Directory -Force $dist | Out-Null

Copy-Item (Join-Path $root "build\VideoEditor.exe") $dist
Copy-Item (Join-Path $root "ffmpeg.exe") $dist
Copy-Item (Join-Path $root "ffprobe.exe") $dist

Write-Host "Deploying Qt libraries..."
& $windeployqt --release --no-translations (Join-Path $dist "VideoEditor.exe")

Write-Host "Copying MinGW runtime dependencies..."
$known = @{}
foreach ($dll in Get-ChildItem $dist -Filter *.dll -File) {
    $known[$dll.Name.ToLowerInvariant()] = $true
}

$queue = New-Object System.Collections.Queue
foreach ($file in Get-ChildItem $dist -Recurse -File) {
    if ($file.Extension -in @(".exe", ".dll")) {
        $queue.Enqueue($file.FullName)
    }
}

$checked = @{}
while ($queue.Count -gt 0) {
    $file = $queue.Dequeue()
    $dependencyLines = & $objdump -p $file 2>$null | Select-String "DLL Name"
    foreach ($line in $dependencyLines) {
        $dependency = ($line.ToString() -split ":", 2)[1].Trim()
        $key = $dependency.ToLowerInvariant()
        if ($checked.ContainsKey($key)) {
            continue
        }
        $checked[$key] = $true

        if ($known.ContainsKey($key)) {
            continue
        }

        $source = Join-Path $mingwBin $dependency
        if (Test-Path $source) {
            Copy-Item $source $dist
            $known[$key] = $true
            $queue.Enqueue((Join-Path $dist $dependency))
        }
    }
}

Write-Host "Done. Run: $dist\VideoEditor.exe"
