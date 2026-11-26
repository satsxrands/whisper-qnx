#!/bin/bash
# Stream audio from the Mac to the QNX VM's voicecmd as raw 16 kHz mono s16le PCM over TCP.
# The Mac LISTENS on PORT (default 5555); voicecmd inside the VM connects out to it as tcp:10.0.2.2:PORT
# (10.0.2.2 is the Mac as seen from a QEMU user-mode network). Start this first, then the VM side (demo-live.sh does both).
#   scripts/mac-mic-stream.sh --list          list audio inputs (avfoundation indices)
#   scripts/mac-mic-stream.sh                 live microphone, system default input (override: MIC=<index from --list>)
#   scripts/mac-mic-stream.sh --wav x.wav     play a WAV as if it were the microphone (real-time paced)
# Needs ffmpeg (already installed via Homebrew). The first live run makes macOS ask Terminal for microphone access.
set -e
PORT="${PORT:-5555}"
FF="$(command -v ffmpeg || true)"; [ -x "$FF" ] || { echo "ffmpeg not found (brew install ffmpeg)" >&2; exit 1; }
case "$1" in
  --list) exec "$FF" -hide_banner -f avfoundation -list_devices true -i "" ;;
  --wav)  [ -f "$2" ] || { echo "usage: $0 --wav file.wav" >&2; exit 2; }
          echo "[mic-stream] playing $2 as the microphone, waiting for the VM on tcp port $PORT" >&2
          exec "$FF" -hide_banner -loglevel warning -re -i "$2" -ac 1 -ar 16000 -f s16le "tcp://0.0.0.0:$PORT?listen=1" ;;
  "")     echo "[mic-stream] live microphone input ${MIC:-default}, waiting for the VM on tcp port $PORT" >&2
          exec "$FF" -hide_banner -loglevel warning -fflags nobuffer -f avfoundation -i ":${MIC:-default}" \
               -ac 1 -ar 16000 -f s16le "tcp://0.0.0.0:$PORT?listen=1" ;;
  *) echo "usage: $0 [--list | --wav file.wav]" >&2; exit 2 ;;
esac
