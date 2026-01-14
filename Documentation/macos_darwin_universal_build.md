# macOS/Darwin Universal Build

This document describes the macOS build pipeline that produces x86_64, arm64,
and universal2 outputs from the existing Xcode project.

## Prerequisites

- Xcode with command line tools installed.

## Build script

```sh
bash scripts/macos-build.sh
```

Optional overrides:

```sh
LLTD_SCHEME=lltdResponder LLTD_CONFIGURATION=Release bash scripts/macos-build.sh
```

## Outputs

- `dist/macos/x86_64/<product>`
- `dist/macos/arm64/<product>`
- `dist/macos/universal2/<product>`

The script validates universal outputs using `lipo -info`.
