#!/bin/bash
# With the VM up (scripts/run-vm-x86_64.sh &), push binaries + model + wav and transcribe. Prints transcript and timings.
set -e
Q="$HOME/qnx"; S="$Q/stage-x86_64/opt"; B="$Q/work/build-x86_64/bin"
rm -rf "$S"; mkdir -p "$S/whisper/bin" "$S/whisper/lib"
cp "$B/whisper-cli" "$S/whisper/bin/"; cp -a "$B"/lib*.so* "$S/whisper/lib/"
cp "$Q/models/ggml-tiny.en.bin" "$Q/models/jfk.wav" "$S/whisper/"
cp "$Q/smoke/hello_x86_64" "$S/"
VM="$(dirname "$0")/vm-ssh.sh"
tar -C "$S" -cf - whisper hello_x86_64 | "$VM" 'mkdir -p /data/w && cd /data/w && tar xf -'
"$VM" 'cd /data/w && ./hello_x86_64 && cd whisper && LD_LIBRARY_PATH=$PWD/lib time ./bin/whisper-cli -m ggml-tiny.en.bin -f jfk.wav -t 4'
