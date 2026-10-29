#!/bin/bash
# Installs the Cruz Pop Mix template into FL Studio's Templates folder, then
# installs any Cruz plugin the template uses that isn't on this Mac yet.
set -eo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
RELEASE="https://github.com/PrideAutoSales/FLStudio/releases/download/latest"
PLUGINS=("Cruz EQ" "Cruz Limiter" "Cruz Opto" "Cruz FET" "Cruz Tape" "Cruz Drive"
         "Cruz Verb" "Cruz Echo" "Cruz Synth" "Cruz Sidechain")

# FL 20 gets its own copy: it has no Hyper Chorus or Vintage Phaser.
shopt -s nullglob
fl20=0
newer=0
for app in /Applications/FL\ Studio*.app; do
    case "$(basename "$app")" in
        "FL Studio 20.app") fl20=1 ;;
        *) newer=1 ;;
    esac
done
templates=()
if [ "$newer" = 1 ] || [ "$fl20" = 0 ]; then templates+=("Cruz Pop Mix"); fi
if [ "$fl20" = 1 ] || [ "$newer" = 0 ]; then templates+=("Cruz Pop Mix (FL 20)"); fi

# FL's user data folder is in Documents unless it was moved (Pedro's
# MacBook Air keeps it in Music). Install into every one that exists.
data_dirs=()
for d in "$HOME/Documents/Image-Line/FL Studio" "$HOME/Music/Image-Line/FL Studio"; do
    if [ -d "$d" ]; then data_dirs+=("$d"); fi
done
if [ "${#data_dirs[@]}" = 0 ]; then data_dirs=("$HOME/Documents/Image-Line/FL Studio"); fi

echo ""
for d in "${data_dirs[@]}"; do
    for t in "${templates[@]}"; do
        dest="$d/Projects/Templates/Cruz/$t"
        rm -rf "$dest"
        mkdir -p "$dest"
        cp -R "$HERE/$t/." "$dest/"
        xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
        echo "Installed template: $dest"
    done
done

missing=()
for p in "${PLUGINS[@]}"; do
    if [ ! -d "/Library/Audio/Plug-Ins/VST3/$p.vst3" ]; then missing+=("$p"); fi
done

if [ "${#missing[@]}" -gt 0 ]; then
    echo ""
    echo "The template also uses these Cruz plugins, which aren't installed yet:"
    printf '  %s\n' "${missing[@]}"
    work="$(mktemp -d)"
    added=0
    for p in "${missing[@]}"; do
        pkg="${p// /}-mac"
        echo ""
        echo "Downloading $p..."
        if curl -fsSL -o "$work/$pkg.zip" "$RELEASE/$pkg.zip"; then
            (cd "$work" && ditto -x -k "$pkg.zip" .)
            bash "$work/$pkg/install.sh"
            added=$((added + 1))
        else
            echo "Couldn't download $p. The template still opens; that slot shows as missing until $p is installed."
        fi
    done
    rm -rf "$work"
fi

echo ""
echo "Done. In FL Studio, open it from File > New from template > Cruz."
if [ "${#templates[@]}" -gt 1 ]; then
    echo "Pick \"Cruz Pop Mix\" in FL 2024 and newer, and \"Cruz Pop Mix (FL 20)\" in FL 20."
fi
if [ "${added:-0}" -gt 0 ]; then
    echo "New plugins were added, so first go to Options > Manage plugins and click Find installed plugins."
fi
