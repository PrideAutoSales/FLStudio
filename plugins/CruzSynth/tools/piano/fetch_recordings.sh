#!/bin/sh
# Download the grand piano recordings the Concert Grand is measured from
# (Salamander Grand Piano V3 by Alexander Holm, CC BY 3.0; about 700 MB of FLAC).
# Usage: sh fetch_recordings.sh <folder>    -> recordings end up in <folder>/Samples
set -e
dest="${1:-salamander}"
git clone --filter=blob:none --no-checkout https://github.com/sfzinstruments/SalamanderGrandPiano "$dest"
cd "$dest"
git checkout 3382bf9496bba2486f5ab0de55a264d1dfc38404
