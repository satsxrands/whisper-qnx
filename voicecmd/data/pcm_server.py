#!/usr/bin/env python3
"""Serve a raw s16le 16 kHz mono PCM file to one TCP client, paced at SPEED x realtime.
The QNX guest reaches the Mac as 10.0.2.2 under QEMU user networking.  usage: pcm_server.py file.pcm [port] [speed]"""
import socket, sys, time
path = sys.argv[1]; port = int(sys.argv[2]) if len(sys.argv) > 2 else 5555; speed = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
data = open(path, "rb").read()
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); srv.bind(("0.0.0.0", port)); srv.listen(1)
c, addr = srv.accept(); print("client", addr, flush=True)
chunk = 3200  # 100 ms
t0 = time.time()
for i in range(0, len(data), chunk):
    c.sendall(data[i:i + chunk])
    due = t0 + (i + chunk) / 32000.0 / speed
    time.sleep(max(0, due - time.time()))
c.close(); print("sent", len(data), "bytes", flush=True)
