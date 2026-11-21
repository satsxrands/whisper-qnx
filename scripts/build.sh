#!/bin/bash
# Cross-compile whisper.cpp for QNX SDP 8.0 inside the licence-bound container.
# Usage: scripts/build.sh [aarch64le|x86_64]   (run on the Mac)
# The licence is bound to the pinned host identity: never change --hostname/--mac-address.
# Large artefacts live outside git: sources and builds under ~/qnx/work.
set -e
ARCH="${1:-aarch64le}"
export PATH="$HOME/.rd/bin:$PATH"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/qnx/work"; mkdir -p "$WORK"
[ -d "$WORK/whisper.cpp" ] || git clone https://github.com/ggml-org/whisper.cpp "$WORK/whisper.cpp"
git -C "$WORK/whisper.cpp" checkout -q -f "$(cat "$HERE/WHISPER_REF")"
for p in "$HERE"/patches/*.patch; do git -C "$WORK/whisper.cpp" apply "$p"; done
docker run --rm -u root -e HOME=/root --platform linux/amd64 --hostname qnx-dev --mac-address 02:42:ac:11:00:42 \
  -v "$HOME/qnx:/qnx" -v "$HOME/qnx/dotroot:/root" -v "$HERE:/repo" qnx800:latest bash -c "
  set -e
  source /qnx/qnx800/qnxsdp-env.sh
  cmake -S /qnx/work/whisper.cpp -B /qnx/work/build-$ARCH -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/repo/cmake/qnx800.toolchain.cmake -DQNX_ARCH=$ARCH \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    -DGGML_NATIVE=OFF -DGGML_LTO=OFF -DGGML_OPENMP=OFF -DGGML_VULKAN=OFF -DWHISPER_SDL2=OFF \
    -DWHISPER_BUILD_TESTS=OFF -DCMAKE_SUPPRESS_REGENERATION=ON
  cmake --build /qnx/work/build-$ARCH -j4 --target whisper-cli
"
echo "Binaries in $WORK/build-$ARCH/bin"
