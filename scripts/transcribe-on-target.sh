#!/bin/bash
# With the VM up (scripts/run-vm-x86_64.sh &), push binaries + model + wav and transcribe. Prints transcript and timings.
set -e
Q="$HOME/qnx"; S="$Q/stage-x86_64/opt"; B="$Q/work/build-x86_64/bin"
rm -rf "$S"; mkdir -p "$S/whisper/bin" "$S/whisper/lib"
cp "$B/whisper-cli" "$S/whisper/bin/"; cp -a "$B"/lib*.so* "$S/whisper/lib/"
cp "$Q/models/ggml-tiny.en.bin" "$Q/models/jfk.wav" "$S/whisper/"
[ -f "$Q/smoke/hello_x86_64" ] && cp "$Q/smoke/hello_x86_64" "$S/"   # optional smoke binary
SSH="$(dirname "$0")/vm-ssh.sh"
tar -C "$S" -cf - whisper $([ -f "$S/hello_x86_64" ] && echo hello_x86_64) | "$SSH" 'mkdir -p /data/home/qnxuser/w && cd /data/home/qnxuser/w && tar xf -'
"$SSH" 'cd /data/home/qnxuser/w && ([ ! -x ./hello_x86_64 ] || ./hello_x86_64) && cd whisper && LD_LIBRARY_PATH=$PWD/lib time ./bin/whisper-cli -m ggml-tiny.en.bin -f jfk.wav -t 4'
