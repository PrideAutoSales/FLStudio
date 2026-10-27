#!/bin/bash
# Installs the plugins in this folder into the Mac's main plugin folders
# (/Library/Audio/Plug-Ins), which is where FL Studio looks for them.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
VST3_DIR="/Library/Audio/Plug-Ins/VST3"
AU_DIR="/Library/Audio/Plug-Ins/Components"

shopt -s nullglob
set -- "$HERE"/*.vst3 "$HERE"/*.component

if [ "$#" -eq 0 ]; then
    echo "No plugins were found next to this script."
    echo "Double-click the downloaded zip first, then run install.sh from the folder it makes."
    exit 1
fi

echo ""
echo "This puts the plugins in /Library/Audio/Plug-Ins, the folder FL Studio scans."
echo "macOS needs your Mac login password to change that folder."
echo "Type it below and press Return. Nothing appears while you type, which is normal."
echo ""
sudo -v -p "Mac password: "

echo ""
for src in "$@"; do
    name="$(basename "$src")"
    case "$name" in
        *.vst3) folder="VST3"; dest_dir="$VST3_DIR" ;;
        *) folder="Components"; dest_dir="$AU_DIR" ;;
    esac

    sudo mkdir -p "$dest_dir"
    sudo rm -rf "$dest_dir/$name"
    sudo cp -R "$src" "$dest_dir/"
    sudo chmod -R a+rX "$dest_dir/$name"

    # Files downloaded from the web are flagged by macOS, which stops FL Studio
    # from loading an unsigned plugin. Clear that flag on this plugin only.
    sudo xattr -dr com.apple.quarantine "$dest_dir/$name" 2>/dev/null || true

    # Older builds went into your own Library folder. Remove that copy so FL
    # Studio does not find two versions of the same plugin.
    sudo rm -rf "$HOME/Library/Audio/Plug-Ins/$folder/$name"

    echo "Installed $dest_dir/$name"
done

# Makes macOS notice the new Audio Units right away.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Done. Open FL Studio, go to Options > Manage plugins, and click Find installed plugins."
