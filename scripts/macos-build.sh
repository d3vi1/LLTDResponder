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

  local args=()
  if [[ -n "$SCHEME" ]]; then
    args=( -project "$PROJECT" -scheme "$SCHEME" -configuration "$CONFIGURATION" -arch "$arch" -sdk macosx )
    xcodebuild "${args[@]}" -derivedDataPath "$derived" build >&2
  else
    args=( -project "$PROJECT" -target "$TARGET" -configuration "$CONFIGURATION" -arch "$arch" -sdk macosx )
    xcodebuild "${args[@]}" SYMROOT="$derived" OBJROOT="$derived" build >&2
  fi

  local settings
  if [[ -n "$SCHEME" ]]; then
    settings=$(xcodebuild "${args[@]}" \
      -derivedDataPath "$derived" \
      -showBuildSettings 2>&1) || {
        echo "xcodebuild -showBuildSettings failed:" >&2
        echo "$settings" >&2
        exit 1
      }
  else
    settings=$(xcodebuild "${args[@]}" \
      SYMROOT="$derived" \
      OBJROOT="$derived" \
      -showBuildSettings 2>&1) || {
        echo "xcodebuild -showBuildSettings failed:" >&2
        echo "$settings" >&2
        exit 1
      }
  fi

  local build_dir
  build_dir=$(printf '%s\n' "$settings" | awk -F '=' '
    /^[[:space:]]*TARGET_BUILD_DIR[[:space:]]*=/ {
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2);
      print $2; exit
    }')

  local product_name
  product_name=$(printf '%s\n' "$settings" | awk -F '=' '
    /^[[:space:]]*FULL_PRODUCT_NAME[[:space:]]*=/ {
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2);
      print $2; exit
    }')

  local executable_path
  executable_path=$(printf '%s\n' "$settings" | awk -F '=' '
    /^[[:space:]]*EXECUTABLE_PATH[[:space:]]*=/ {
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2);
      print $2; exit
    }')

  echo "DEBUG: build_dir='$build_dir'" >&2
  echo "DEBUG: product_name='$product_name'" >&2
  echo "DEBUG: executable_path='$executable_path'" >&2

  if [[ -z "$build_dir" ]]; then
    echo "Failed to determine TARGET_BUILD_DIR from xcodebuild settings." >&2
    echo "First 60 lines of captured settings:" >&2
    printf '%s\n' "$settings" | head -60 >&2
    exit 1
  fi

  if [[ -z "$product_name" ]]; then
    echo "Failed to determine FULL_PRODUCT_NAME from xcodebuild settings." >&2
    exit 1
  fi

  local product="$build_dir/$product_name"
  echo "DEBUG: product='$product'" >&2
  echo "DEBUG: checking if product exists..." >&2
  if [[ ! -e "$product" ]]; then
    echo "Product not found: $product" >&2
    exit 1
  fi
  echo "DEBUG: product exists, about to echo result..." >&2

  echo "$product|$executable_path"
  echo "DEBUG: echo completed" >&2
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
