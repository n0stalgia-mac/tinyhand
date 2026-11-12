#!/bin/sh
# Build classic TinyHand for 68000 and PowerPC, then copy disk images into dist/.
set -eu
cd "$(dirname "$0")"

M68K_TOOL="/home/grok/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake"
PPC_TOOL="/home/grok/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake"

cmake -S classic -B build-68k -DCMAKE_TOOLCHAIN_FILE="$M68K_TOOL"
cmake --build build-68k

cmake -S classic -B build-ppc -DCMAKE_TOOLCHAIN_FILE="$PPC_TOOL"
cmake --build build-ppc

mkdir -p dist
cp -f build-68k/TinyHand.bin dist/TinyHand-68k.bin
cp -f build-68k/TinyHand.dsk dist/TinyHand-68k.dsk
cp -f build-ppc/TinyHand.bin dist/TinyHand-ppc.bin
cp -f build-ppc/TinyHand.dsk dist/TinyHand-ppc.dsk

test -s dist/TinyHand-68k.bin
test -s dist/TinyHand-68k.dsk
test -s dist/TinyHand-ppc.bin
test -s dist/TinyHand-ppc.dsk
