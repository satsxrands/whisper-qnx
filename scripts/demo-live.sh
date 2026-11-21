#!/bin/bash
# Live in-car demo: Mac microphone (or a WAV) -> TCP -> voicecmd in the QNX 8.0 VM -> JSON intents -> dashboard in the browser.
#   scripts/run-vm-quickstart.sh > /dev/null 2>&1 &      (boot the VM first, wait about 90 s; scripts/stage-vm.sh once)
#   scripts/demo-live.sh                  live microphone
#   scripts/demo-live.sh --wav x.wav      play a WAV as the microphone
# Ctrl-C stops everything. Dashboard: http://127.0.0.1:8090
HERE="$(cd "$(dirname "$0")/.." && pwd)"; PORT="${PORT:-5555}"
VM=quickstart "$HERE/scripts/vm-ssh.sh" true 2>/dev/null || { echo "VM not reachable on ssh localhost:2223: boot it with scripts/run-vm-quickstart.sh" >&2; exit 1; }
VM=quickstart "$HERE/scripts/vm-ssh.sh" 'test -x /data/home/qnxuser/x/gen/voicecmd' 2>/dev/null || { echo "run scripts/stage-vm.sh first" >&2; exit 1; }
pkill -f "ffmpeg.*tcp://0.0.0.0:$PORT" 2>/dev/null
PORT=$PORT "$HERE/scripts/mac-mic-stream.sh" "$@" & MIC=$!
trap 'kill $MIC 2>/dev/null; pkill -f "ffmpeg.*tcp://0.0.0.0:$PORT" 2>/dev/null; exit 0' INT TERM EXIT
sleep 2; [ -n "$NOOPEN" ] || (sleep 1; open http://127.0.0.1:8090 2>/dev/null) &
VM=quickstart "$HERE/scripts/vm-ssh.sh" "cd /data/home/qnxuser/x/gen && LD_LIBRARY_PATH=\$PWD/lib ./voicecmd -m ../vc/ggml-tiny.en.bin -a tcp:10.0.2.2:$PORT -t 4 2>/dev/null" \
  | python3 -I "$HERE/demo/dashboard.py" --tee "${TEE:-/tmp/voicecmd-demo-events.jsonl}"
