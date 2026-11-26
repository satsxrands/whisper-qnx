#!/bin/bash
# Live in-car demo against the real QNX 8.0 instance on AWS: Mac mic (or WAV) -> ffmpeg listens on 127.0.0.1:PORT ->
# ssh reverse tunnel -> voicecmd on QNX (tcp:127.0.0.1:PORT) -> JSON intents back over ssh -> dashboard on http://127.0.0.1:8090
#   scripts/demo-live-aws.sh                live microphone (macOS asks Terminal for mic access on first run)
#   scripts/demo-live-aws.sh --wav x.wav    WAV played as the microphone
HERE="$(cd "$(dirname "$0")/.." && pwd)"; PORT="${PORT:-5555}"
"$HERE/scripts/aws-ssh.sh" 'echo ok' >/dev/null 2>&1 || { echo "AWS instance not reachable: start it (see STATE.md), update ~/qnx/aws/ip" >&2; exit 1; }
"$HERE/scripts/aws-ssh.sh" 'test -x x/gen/voicecmd' || { echo "run scripts/stage-aws.sh first" >&2; exit 1; }
pkill -f "ffmpeg.*tcp://0.0.0.0:$PORT" 2>/dev/null
PORT=$PORT "$HERE/scripts/mac-mic-stream.sh" "$@" & MIC=$!
trap 'kill $MIC 2>/dev/null; pkill -f "ffmpeg.*tcp://0.0.0.0:$PORT" 2>/dev/null; exit 0' INT TERM EXIT
sleep 2; [ -n "$NOOPEN" ] || (sleep 1; open http://127.0.0.1:8090 2>/dev/null) &
"$HERE/scripts/aws-ssh.sh" -R "$PORT:127.0.0.1:$PORT" "cd x/gen && LD_LIBRARY_PATH=\$PWD/lib ./voicecmd -m ../vc/ggml-tiny.en.bin -a tcp:127.0.0.1:$PORT -t 4 2>/dev/null" \
  | python3 -I "$HERE/demo/dashboard.py" --tee "${TEE:-/tmp/voicecmd-demo-events.jsonl}"
