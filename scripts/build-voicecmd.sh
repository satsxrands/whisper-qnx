#!/bin/bash
# Cross-compile the voicecmd daemon + grammar test for QNX 8.0 against the already built whisper.cpp libs.
# Usage: scripts/build-voicecmd.sh [aarch64le|x86_64]  (run build.sh for the same arch first)
set -e
ARCH="${1:-x86_64}"
export PATH="$HOME/.rd/bin:$PATH"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
docker run --rm -u root -e HOME=/root --platform linux/amd64 --hostname qnx-dev --mac-address 02:42:ac:11:00:42 \
  -v "$HOME/qnx:/qnx" -v "$HOME/qnx/dotroot:/root" -v "$HERE:/repo" qnx800:latest bash -c "
  set -e
  source /qnx/qnx800/qnxsdp-env.sh
  cmake -S /repo/voicecmd -B /qnx/work/vc-$ARCH -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/repo/cmake/qnx800.toolchain.cmake -DQNX_ARCH=$ARCH -DCMAKE_BUILD_TYPE=Release \
    -DWHISPER_DIR=/qnx/work/whisper.cpp -DWHISPER_BUILD=/qnx/work/build-$ARCH
  cmake --build /qnx/work/vc-$ARCH
"
echo "Binaries in $HOME/qnx/work/vc-$ARCH"
