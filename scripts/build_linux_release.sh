#!/usr/bin/env bash
# Builds the portable Linux release of Road Traffic Tuner.
#
# Uses the Zig compiler to target glibc 2.17, so the plugin runs on
# essentially any Linux that X-Plane 12 supports, with the C++ runtime
# bundled and only X-Plane's five plugin functions exported.
#
# Requirements:
#   - Zig: either `zig` on the PATH, or the Python package
#     (pip install ziglang), which this script runs as `python3 -m ziglang`
#   - The X-Plane SDK in a folder named SDK inside this project or next to
#     it, or set XPSDK=/path/to/SDK
#
# Usage (from anywhere):   scripts/build_linux_release.sh
# Output:                  build-linux/RoadTrafficTuner/lin_x64/RoadTrafficTuner.xpl

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT/build-linux/RoadTrafficTuner/lin_x64"
OUT="$OUT_DIR/RoadTrafficTuner.xpl"

# --- find the SDK
if [ -z "${XPSDK:-}" ]; then
  for candidate in "$ROOT/SDK" "$ROOT/../SDK"; do
    if [ -f "$candidate/CHeaders/XPLM/XPLMPlugin.h" ]; then
      XPSDK="$(cd "$candidate" && pwd)"
      break
    fi
  done
fi
if [ -z "${XPSDK:-}" ] || [ ! -f "$XPSDK/CHeaders/XPLM/XPLMPlugin.h" ]; then
  echo "X-Plane SDK not found. Put a folder named SDK inside this project or" >&2
  echo "next to it, or run with XPSDK=/path/to/SDK." >&2
  exit 1
fi
echo "Using X-Plane SDK at $XPSDK"

# --- find Zig
if command -v zig >/dev/null 2>&1; then
  ZIG=(zig)
elif python3 -c "import ziglang" >/dev/null 2>&1; then
  ZIG=(python3 -m ziglang)
else
  echo "Zig not found. Install it, or run: pip install ziglang" >&2
  exit 1
fi

# --- build
mkdir -p "$OUT_DIR"
"${ZIG[@]}" c++ \
  -target x86_64-linux-gnu.2.17 \
  -std=c++17 -O2 -s -fPIC -shared -fvisibility=hidden \
  -Wall -Wextra \
  -DAPL=0 -DIBM=0 -DLIN=1 \
  -DXPLM200=1 -DXPLM210=1 -DXPLM300=1 -DXPLM301=1 -DXPLM303=1 \
  -I"$XPSDK/CHeaders/XPLM" -I"$XPSDK/CHeaders/Widgets" \
  -Wl,--version-script="$ROOT/scripts/linux_exports.map" \
  -o "$OUT" \
  "$ROOT/src/rtt_plugin.cpp" "$ROOT/src/rtt_core.cpp"

echo "Built $OUT"

# --- check the exports (skipped if objdump is not installed)
if command -v objdump >/dev/null 2>&1; then
  exports=$(objdump -T "$OUT" | awk '$0 !~ /\*UND\*/ && ($0 ~ / DF / || $0 ~ / DO /) {print $NF}' | sort | tr '\n' ' ')
  expected="XPluginDisable XPluginEnable XPluginReceiveMessage XPluginStart XPluginStop "
  if [ "$exports" = "$expected" ]; then
    echo "Exports OK: $exports"
  else
    echo "Unexpected exports: $exports" >&2
    exit 1
  fi
  newest_glibc=$(objdump -T "$OUT" | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -1)
  echo "Newest glibc symbol required: ${newest_glibc:-none}"
fi
