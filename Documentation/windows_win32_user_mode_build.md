# Windows Win32 User-Mode Build

This document describes the Windows user-mode build that relies only on
Windows SDK headers and libraries (no WDK/DDK).

## Prerequisites

- Visual Studio 2022 or Build Tools with MSVC.
- Windows SDK.
- CMake 3.16+.

## Build steps

```powershell
cmake -S lltdDaemon/win32 -B build\win32 -G "Visual Studio 17 2022" -A x64
cmake --build build\win32 --config Release
```

## Outputs

- `dist/windows/win32/lltd_win32.exe` (when using `scripts/windows-build.ps1`)

## Notes

The binary is a user-mode executable and does not require any WDK/DDK
components.
