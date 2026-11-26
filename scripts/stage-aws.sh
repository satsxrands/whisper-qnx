#!/bin/bash
# Push the aarch64le voicecmd, libs, model and test streams to ~/x on the AWS QNX instance.
# Build first: scripts/build.sh aarch64le && scripts/build-voicecmd.sh aarch64le
set -e
Q="$HOME/qnx"; S="$Q/work/stage-a64"; rm -rf "$S"; mkdir -p "$S/gen/lib" "$S/vc"
cp "$Q/work/vc-aarch64le/voicecmd" "$S/gen/"; cp -a "$Q"/work/build-aarch64le/bin/lib*.so* "$S/gen/lib/"; cp "$Q/models/ggml-tiny.en.bin" "$S/vc/"
cp "$Q"/work/demo/stream[ABCD].wav "$S/vc/" 2>/dev/null || true
COPYFILE_DISABLE=1 tar -C "$S" -cf - gen vc | "$(dirname "$0")/aws-ssh.sh" 'mkdir -p x && cd x && tar xf - && ls gen vc'
