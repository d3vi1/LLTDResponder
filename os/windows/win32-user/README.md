# Win32 User-Mode Build

This directory contains the CMake build files for the Windows user-mode LLTD
binary. It relies only on Windows SDK headers and libraries and does not use
the WDK/DDK.

## Build (Windows)

```powershell
cmake -S os/windows/win32-user -B build\win32 -G "Visual Studio 17 2022" -A x64
cmake --build build\win32 --config Release
```

The output binary is `lltd_win32.exe`.
