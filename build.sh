#!/bin/sh
# Build the 68000 classic Mac TinyHand and copy the disk images into dist/.
set -eu
cd "$(dirname "$0")"

TOOL="/home/grok/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake"

cmake -S classic -B build-68k -DCMAKE_TOOLCHAIN_FILE="$TOOL"
cmake --build build-68k

mkdir -p dist
cp -f build-68k/TinyHand.bin dist/TinyHand-68k.bin
cp -f build-68k/TinyHand.dsk dist/TinyHand-68k.dsk

test -s dist/TinyHand-68k.bin
test -s dist/TinyHand-68k.dsk
