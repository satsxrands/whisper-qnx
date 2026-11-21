#!/bin/bash
# Push voicecmd, its libs and the model to the quickstart VM (/data/home/qnxuser/x). Build first: build.sh x86_64 && build-voicecmd.sh x86_64.
set -e
Q="$HOME/qnx"; S="$Q/work/stage-live"; rm -rf "$S"; mkdir -p "$S/gen/lib" "$S/vc"
cp "$Q/work/vc-x86_64/voicecmd" "$S/gen/"; cp -a "$Q"/work/build-x86_64/bin/lib*.so* "$S/gen/lib/"; cp "$Q/models/ggml-tiny.en.bin" "$S/vc/"
COPYFILE_DISABLE=1 tar -C "$S" -cf - gen vc | VM=quickstart "$(dirname "$0")/vm-ssh.sh" 'mkdir -p /data/home/qnxuser/x && cd /data/home/qnxuser/x && tar xf - 2>/dev/null; ls /data/home/qnxuser/x/gen'
