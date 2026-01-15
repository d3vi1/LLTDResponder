#!/usr/bin/env bash
set -euo pipefail

CORE_DIR="lltdResponder"

if [ ! -d "$CORE_DIR" ]; then
  echo "Core directory '$CORE_DIR' not found." >&2
  exit 1
fi

CODE_GLOBS=(
  "-g" "*.c"
  "-g" "*.h"
  "-g" "*.m"
  "-g" "*.mm"
  "-g" "*.cc"
  "-g" "*.cpp"
  "-g" "*.hpp"
)

OS_MACRO_REGEX='(__APPLE__|__linux__|_WIN32|__WIN32|__FreeBSD__|__sun|__sunos|__SVR4|__VMKERNEL__|__ESXI__|ESP_PLATFORM)'
OS_HEADER_REGEX='(<windows\.h>|<CoreFoundation/|<CoreServices/|<IOKit/|<mach/|<SystemConfiguration/|<linux/|<net/|<ifaddrs\.h>|<arpa/inet\.h>|<syslog\.h>)'

search() {
  if command -v rg >/dev/null 2>&1; then
    rg -n --hidden --no-heading "${CODE_GLOBS[@]}" "$@"
  else
    # shellcheck disable=SC2016
    find "$CORE_DIR" -type f \( -name '*.c' -o -name '*.h' -o -name '*.m' -o -name '*.mm' -o -name '*.cc' -o -name '*.cpp' -o -name '*.hpp' \) -print0 \
      | xargs -0 grep -nE "$@"
  fi
}

echo "==> Checking for OS-specific macros under $CORE_DIR/"
if search "$OS_MACRO_REGEX" "$CORE_DIR" >/dev/null 2>&1; then
  echo "ERROR: OS-specific conditional compilation macros found under $CORE_DIR/:" >&2
  search "$OS_MACRO_REGEX" "$CORE_DIR" >&2 || true
  exit 1
fi

echo "==> Checking for OS-specific headers under $CORE_DIR/"
if search "$OS_HEADER_REGEX" "$CORE_DIR" >/dev/null 2>&1; then
  echo "ERROR: OS-specific headers found under $CORE_DIR/:" >&2
  search "$OS_HEADER_REGEX" "$CORE_DIR" >&2 || true
  exit 1
fi

echo "OK: $CORE_DIR/ looks OS-agnostic."

