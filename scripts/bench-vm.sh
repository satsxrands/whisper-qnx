#!/bin/bash
# Run voicecmd inside the quickstart VM on a staged 20-phrase WAV stream and score it.
# usage: scripts/bench-vm.sh <stream.wav name in /data/home/qnxuser/x/vc> <label> [voicecmd args...]
# Staging layout on the VM: /data/home/qnxuser/x/{vc/ggml-tiny.en.bin,vc/streamA.wav,gen/voicecmd,gen/lib}; see README "Latency tuning".
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; S="$1"; L="$2"; shift 2
ARGS="$(printf "%q " "$@")"
OUT="${OUT:-$HOME/qnx/work/demo}/bench_${L}_${S%.wav}.jsonl"
VM=quickstart "$HERE/scripts/vm-ssh.sh" "cd /data/home/qnxuser/x/gen && LD_LIBRARY_PATH=\$PWD/lib ./voicecmd -m ../vc/ggml-tiny.en.bin -a file:../vc/$S $ARGS 2>/dev/null" > "$OUT" 2>/dev/null
echo "== $L $S  args: $*"; python3 -I "$HERE/voicecmd/data/score.py" "$HERE/voicecmd/data/commands.tsv" "$OUT" | grep -E "FAIL|MISSING|accuracy|latency"
