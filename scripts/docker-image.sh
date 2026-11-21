#!/bin/bash
# Build the QNX build image (needs NO SDP; the SDP is mounted from $HOME at run time). Rancher Desktop (moby) or Docker.
set -e
export PATH="$HOME/.rd/bin:$PATH"
cd "$(dirname "$0")/../docker/qnx-build"
docker build -t qnx800 --platform linux/amd64 \
  --build-arg USER_NAME="$(id -un)" --build-arg GROUP_NAME="$(id -gn)" \
  --build-arg USER_ID="$(id -u)" --build-arg GROUP_ID="$(id -g)" \
  --build-arg QNX_SDP_VERSION=qnx800 .
