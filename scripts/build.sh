#!/bin/bash
# Cross-compile whisper.cpp for QNX 8.0 inside the qnx800 container. Needs ~/qnx800 (SDP + Linux host tools).
# Usage: scripts/build.sh [aarch64le|x86_64]   (run on the Mac; it execs into the container)
set -e
ARCH="${1:-x86_64}"
export PATH="$HOME/.rd/bin:$PATH"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$HOME/qnx800/qnxsdp-env.sh" ] || { echo "SDP missing: install QNX SDP 8.0 + Host Tools (Linux 64-bit) to ~/qnx800 first"; exit 2; }
if ! docker ps -a --format '{{.Names}}' | grep -qx qnx-build; then
  docker create -it --name=qnx-build --platform linux/amd64 --net=host --privileged \
    -v "$HOME:/home/$USER/" qnx800:latest /bin/bash --rcfile /usr/local/qnx/.qnxbashrc
fi
docker start qnx-build >/dev/null
REL="${HERE#$HOME/}"
docker exec -i qnx-build bash -lc "
  set -e
  source /home/$USER/qnx800/qnxsdp-env.sh
  cd /home/$USER/$REL
  [ -d third_party/whisper.cpp ] || git clone https://github.com/ggml-org/whisper.cpp third_party/whisper.cpp
  git -C third_party/whisper.cpp checkout \$(cat WHISPER_REF)
  cmake -S third_party/whisper.cpp -B build-$ARCH -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=\$PWD/cmake/qnx800.toolchain.cmake -DQNX_ARCH=$ARCH \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DGGML_NATIVE=OFF -DGGML_LTO=OFF -DGGML_VULKAN=OFF -DWHISPER_SDL2=OFF \
    -DCMAKE_SUPPRESS_REGENERATION=ON
  cmake --build build-$ARCH -j1
"
echo "Binaries in build-$ARCH/bin"
