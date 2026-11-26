#!/bin/bash
# Same benchmark as bench-vm.sh, on the real QNX 8.0 aarch64 AWS instance. usage: scripts/bench-aws.sh <streamX.wav> <label> [voicecmd args...]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; S="$1"; L="$2"; shift 2
ARGS="$(printf "%q " "$@")"
OUT="${OUT:-$HOME/qnx/work/demo/aws}"; mkdir -p "$OUT"; F="$OUT/${L}_${S%.wav}.jsonl"
"$HERE/scripts/aws-ssh.sh" "cd x/gen && LD_LIBRARY_PATH=\$PWD/lib ./voicecmd -m ../vc/ggml-tiny.en.bin -a file:../vc/$S $ARGS 2>/dev/null" > "$F"
echo "== $L $S  args: $*"; python3 -I "$HERE/voicecmd/data/score.py" "$HERE/voicecmd/data/commands.tsv" "$F" | grep -E "FAIL|MISSING|accuracy|latency"
