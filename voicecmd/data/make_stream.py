#!/usr/bin/env python3
"""Concatenate per-command WAVs (16 kHz mono 16-bit) into one stream with silence+faint noise gaps.
usage: make_stream.py out.wav in1.wav in2.wav ...   Also writes out.pcm (raw s16le) for the tcp backend."""
import sys, wave, random, struct
random.seed(7)
out = sys.argv[1]; ins = sys.argv[2:]
def noise(sec, amp=8):
    return b"".join(struct.pack("<h", random.randint(-amp, amp)) for _ in range(int(16000 * sec)))
pcm = noise(1.0)
for f in ins:
    with wave.open(f) as w:
        assert w.getframerate() == 16000 and w.getnchannels() == 1 and w.getsampwidth() == 2
        pcm += w.readframes(w.getnframes())
    pcm += noise(1.5)
with wave.open(out, "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000); w.writeframes(pcm)
open(out.rsplit(".", 1)[0] + ".pcm", "wb").write(pcm)
print(len(pcm) / 32000, "seconds")
