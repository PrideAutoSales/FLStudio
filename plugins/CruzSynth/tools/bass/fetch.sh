#!/bin/sh
# Downloads the recordings the Bass Guitar is measured from:
# Black And Blue Basses v1.002 by Karoryfer Samples, released to the public domain (CC0).
# https://github.com/sfzinstruments/karoryfer.black-and-blue-basses
set -e
dir=${1:-recordings}
mkdir -p "$dir"
cd "$dir"
curl -L -o bb.zip https://github.com/sfzinstruments/karoryfer.black-and-blue-basses/releases/download/v1.002/Black_And_Blue_Basses_1002.zip
unzip -q -o bb.zip 'Samples/darkblack/reg/*' 'Samples/darkblack/rel/*' 'license*'
echo "recordings in $dir/Samples/darkblack"
