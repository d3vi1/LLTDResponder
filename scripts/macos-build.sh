#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/lltdDaemon.xcodeproj"
SCHEME="${LLTD_SCHEME:-}"
TARGET="${LLTD_TARGET:-lltdDaemon}"
CONFIGURATION="${LLTD_CONFIGURATION:-Release}"

function build_flags() {
  local arch="$1"
  if [[ -n "$SCHEME" ]]; then
    echo "-project" "$PROJECT" "-scheme" "$SCHEME" "-configuration" "$CONFIGURATION" "-arch" "$arch" "-sdk" "macosx"
  else
    echo "-project" "$PROJECT" "-target" "$TARGET" "-configuration" "$CONFIGURATION" "-arch" "$arch" "-sdk" "macosx"
  fi
}

function build_arch() {
  local arch="$1"
  local derived="$ROOT_DIR/build/macos/$arch"

  if [[ -n "$SCHEME" ]]; then
    xcodebuild $(build_flags "$arch") \
      -derivedDataPath "$derived" \
      build
  else
    # Use build settings (not env vars) for target-based builds
    xcodebuild $(build_flags "$arch") \
      SYMROOT="$derived" \
      OBJROOT="$derived" \
      build
  fi

  local settings
  if [[ -n "$SCHEME" ]]; then
    settings=$(xcodebuild $(build_flags "$arch") \
      -derivedDataPath "$derived" \
      -showBuildSettings)
  else
    settings=$(xcodebuild $(build_flags "$arch") \
      SYMROOT="$derived" \
      OBJROOT="$derived" \
      -showBuildSettings)
  fi

  local build_dir
  build_dir=$(echo "$settings" | awk -F ' = ' '/TARGET_BUILD_DIR/ {print $2; exit}')
  local product_name
  product_name=$(echo "$settings" | awk -F ' = ' '/FULL_PRODUCT_NAME/ {print $2; exit}')
  local executable_path
  executable_path=$(echo "$settings" | awk -F ' = ' '/EXECUTABLE_PATH/ {print $2; exit}')

  if [[ -z "$build_dir" || -z "$product_name" ]]; then
    echo "Failed to determine build output path" >&2
    exit 1
  fi

  local product="$build_dir/$product_name"
  if [[ ! -e "$product" ]]; then
    echo "Product not found: $product" >&2
    exit 1
  fi

  echo "$product|$executable_path"
}

mkdir -p "$ROOT_DIR/dist/macos/x86_64" "$ROOT_DIR/dist/macos/arm64" "$ROOT_DIR/dist/macos/universal2"

x86_result=$(build_arch "x86_64")
arm_result=$(build_arch "arm64")

x86_product=${x86_result%%|*}
arm_product=${arm_result%%|*}
exe_path=${x86_result##*|}

function copy_product() {
  local src="$1"
  local dest="$2"
  if [[ -d "$src" ]]; then
    rm -rf "$dest"
    cp -R "$src" "$dest"
  else
    cp "$src" "$dest"
  fi
}

product_name=$(basename "$x86_product")

copy_product "$x86_product" "$ROOT_DIR/dist/macos/x86_64/$product_name"
copy_product "$arm_product" "$ROOT_DIR/dist/macos/arm64/$product_name"

if [[ "$product_name" == *.app ]]; then
  universal_bundle="$ROOT_DIR/dist/macos/universal2/$product_name"
  rm -rf "$universal_bundle"
  cp -R "$x86_product" "$universal_bundle"

  if [[ -z "$exe_path" ]]; then
    echo "Missing EXECUTABLE_PATH for bundle" >&2
    exit 1
  fi

  x86_exec="$x86_product/$exe_path"
  arm_exec="$arm_product/$exe_path"
  universal_exec="$universal_bundle/$exe_path"
  lipo -create "$x86_exec" "$arm_exec" -output "$universal_exec"
  lipo -info "$universal_exec"
else
  universal_binary="$ROOT_DIR/dist/macos/universal2/$product_name"
  lipo -create "$x86_product" "$arm_product" -output "$universal_binary"
  lipo -info "$universal_binary"
fi
