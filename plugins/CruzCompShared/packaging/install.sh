#!/bin/bash
# Copies the plugin in this folder into your user plugin folders so FL Studio can find it.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

installed=""
shopt -s nullglob
for src in "$HERE"/*.vst3 "$HERE"/*.component; do
    name="$(basename "$src")"
    case "$name" in
        *.vst3) dest="$VST3_DIR/$name" ;;
        *) dest="$AU_DIR/$name" ;;
    esac
    rm -rf "$dest"
    cp -R "$src" "$dest"
    # Files downloaded from the web are flagged by macOS, which stops FL Studio
    # from loading an unsigned plugin. Clear that flag on this plugin only.
    xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    installed="${name%.*}"
done

# Makes macOS notice the new Audio Unit right away.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "$installed is installed. Now open FL Studio and rescan your plugins."
