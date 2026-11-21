#!/bin/bash
# Cross-compile the voicecmd daemon + grammar test for QNX 8.0 against the already built whisper.cpp libs.
# Usage: scripts/build-voicecmd.sh [aarch64le|x86_64]  (run build.sh for the same arch first)
set -e
ARCH="${1:-x86_64}"
# SIMD=avx2: link against the build-x86_64-avx2 whisper libs (see build.sh); output vc-x86_64-avx2
SUF=""; [ "$SIMD" = avx2 ] && SUF="-avx2"
# SND=1: build the io-snd capture backend (needs the sound packages: alsa/asoundlib.h + libasound)
SNDFLAG=""; [ -n "$SND" ] && SNDFLAG="-DHAVE_QNX_SND=ON"
export PATH="$HOME/.rd/bin:$PATH"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
docker run --rm -u root -e HOME=/root --platform linux/amd64 --hostname qnx-dev --mac-address 02:42:ac:11:00:42 \
  -v "$HOME/qnx:/qnx" -v "$HOME/qnx/dotroot:/root" -v "$HERE:/repo" qnx800:latest bash -c "
  set -e
  source /qnx/qnx800/qnxsdp-env.sh
  cmake -S /repo/voicecmd -B /qnx/work/vc-$ARCH$SUF -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/repo/cmake/qnx800.toolchain.cmake -DQNX_ARCH=$ARCH -DCMAKE_BUILD_TYPE=Release \
    $SNDFLAG -DWHISPER_DIR=/qnx/work/whisper.cpp -DWHISPER_BUILD=/qnx/work/build-$ARCH$SUF
  cmake --build /qnx/work/vc-$ARCH$SUF
"
echo "Binaries in $HOME/qnx/work/vc-$ARCH$SUF"
