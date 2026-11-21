#!/usr/bin/env python3
"""Mix cabin-like noise (low-passed white noise + 120 Hz hum) into a 16 kHz mono wav at a given SNR in dB.
usage: add_noise.py in.wav out.wav snr_db"""
import sys, wave, array, math, random
random.seed(11)
inp, out, snr = sys.argv[1], sys.argv[2], float(sys.argv[3])
with wave.open(inp) as w: x = array.array("h", w.readframes(w.getnframes()))
sig = math.sqrt(sum(v * v for v in x) / len(x))
n = []; lp = 0.0
for i in range(len(x)):
    lp += 0.1 * (random.uniform(-1, 1) - lp)
    n.append(lp + 0.5 * math.sin(2 * math.pi * 120 * i / 16000))
nr = math.sqrt(sum(v * v for v in n) / len(n))
g = sig / (10 ** (snr / 20)) / nr
y = array.array("h", (max(-32768, min(32767, int(v + g * m))) for v, m in zip(x, n)))
with wave.open(out, "wb") as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000); w.writeframes(y.tobytes())
