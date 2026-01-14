$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\win32"
$outDir = Join-Path $buildDir "out"
$distDir = Join-Path $root "dist\windows\win32"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

cmake -S "$root\os\windows\win32-user" -B $buildDir -G "Visual Studio 17 2022" -A x64 `
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$outDir" `
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=$outDir"
cmake --build $buildDir --config Release

$exePath = Join-Path $outDir "lltd_win32.exe"
if (!(Test-Path $exePath)) {
    $exePath = Join-Path $buildDir "Release\lltd_win32.exe"
}
if (!(Test-Path $exePath)) {
    $candidates = Get-ChildItem -Path $buildDir -Recurse -Filter "lltd_win32.exe" -ErrorAction SilentlyContinue
    if ($candidates.Count -gt 0) {
        $exePath = $candidates[0].FullName
    }
}
if (!(Test-Path $exePath)) {
    throw "Build output not found: $exePath"
}
Copy-Item -Force $exePath (Join-Path $distDir "lltd_win32.exe")

$ctestFile = Join-Path $buildDir "CTestTestfile.cmake"
if (Test-Path $ctestFile) {
    ctest --test-dir $buildDir --config Release
}
