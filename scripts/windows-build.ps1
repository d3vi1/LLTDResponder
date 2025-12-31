$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\win32"
$distDir = Join-Path $root "dist\windows\win32"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

cmake -S "$root\lltdDaemon\win32" -B $buildDir -G "Visual Studio 17 2022" -A x64
cmake --build $buildDir --config Release

$exePath = Join-Path $buildDir "Release\lltd_win32.exe"
if (!(Test-Path $exePath)) {
    $exePath = Join-Path $buildDir "Release\lltd_win32.exe"
}
if (!(Test-Path $exePath)) {
    throw "Build output not found: $exePath"
}
Copy-Item -Force $exePath (Join-Path $distDir "lltd_win32.exe")

$ctestFile = Join-Path $buildDir "CTestTestfile.cmake"
if (Test-Path $ctestFile) {
    ctest --test-dir $buildDir --config Release
}
