#!/bin/bash
# Copies Cruz Patina into your user plugin folders so FL Studio can find it.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
NAME="Cruz Patina"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

mkdir -p "$VST3_DIR" "$AU_DIR"
rm -rf "$VST3_DIR/$NAME.vst3" "$AU_DIR/$NAME.component"
cp -R "$HERE/$NAME.vst3" "$VST3_DIR/"
cp -R "$HERE/$NAME.component" "$AU_DIR/"

# Files downloaded from the web are flagged by macOS, which stops FL Studio
# from loading an unsigned plugin. Clear that flag on our two plugins only.
xattr -dr com.apple.quarantine "$VST3_DIR/$NAME.vst3" "$AU_DIR/$NAME.component" 2>/dev/null || true

# Makes macOS notice the new Audio Unit right away.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "$NAME is installed. Now open FL Studio and rescan your plugins."
