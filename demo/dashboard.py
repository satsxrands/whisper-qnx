#!/usr/bin/env python3
"""In-car dashboard for the voicecmd demo. Reads voicecmd JSON event lines on stdin (piped from the QNX VM over ssh),
keeps cabin state, and serves a live page on http://127.0.0.1:8090 (server-sent events, stdlib only).
usage: <voicecmd event stream> | python3 -I demo/dashboard.py [--port 8090] [--tee events.jsonl]"""
import json, sys, threading, time, argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

STATE = {"temp": 21, "nav": None, "volume": 5, "muted": False, "call": None,
         "windows": {"driver": "closed", "passenger": "closed", "rear": "closed", "front": "closed"},
         "status": "waiting for the VM", "log": [], "n": 0, "v": 0}
LOCK = threading.Lock(); CV = threading.Condition(LOCK)

def apply(ev):
    s = STATE; e = ev.get("event"); s["v"] += 1
    if e == "ready": s["status"] = "listening (model loaded in %.1f s)" % (ev.get("model_load_ms", 0) / 1000)
    elif e == "eof": s["status"] = "audio stream ended"
    elif e == "error": s["status"] = "error: %s" % ev.get("what")
    elif e in ("command", "rejected"):
        s["status"] = "listening"
        ok = e == "command"; sl = ev.get("slots", {}) if ok else {}; it = ev.get("intent", "")
        if ok:
            if it == "set_temperature": s["temp"] = sl.get("celsius", s["temp"])
            elif it == "navigate": s["nav"] = sl.get("destination")
            elif it == "call": s["call"] = sl.get("contact")
            elif it == "volume_up": s["volume"] = min(10, s["volume"] + 1); s["muted"] = False
            elif it == "volume_down": s["volume"] = max(0, s["volume"] - 1)
            elif it == "mute": s["muted"] = not s["muted"]
            elif it in ("open_window", "close_window"):
                w = sl.get("which", "all"); v = "open" if it == "open_window" else "closed"
                for k in (s["windows"] if w == "all" else [w]):
                    if k in s["windows"]: s["windows"][k] = v
        s["n"] += 1
        s["log"].insert(0, {"t": time.strftime("%H:%M:%S"), "ok": ok, "heard": ev.get("transcript", ""),
                            "what": (it + " " + json.dumps(sl)) if ok else "rejected: " + str(ev.get("reason")),
                            "ms": ev.get("latency_ms", 0)})
        del s["log"][12:]

def snapshot(): return json.dumps(STATE)

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/events":
            self.send_response(200); self.send_header("Content-Type", "text/event-stream"); self.send_header("Cache-Control", "no-cache"); self.end_headers()
            last = -1
            try:
                while True:
                    with CV:
                        CV.wait_for(lambda: STATE["v"] != last, timeout=15)
                        data = snapshot(); last = STATE["v"]
                    self.wfile.write(("data: " + data + "\n\n").encode()); self.wfile.flush()
            except OSError: return
        body = PAGE.encode(); self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)

PAGE = r"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>QNX in-car voice demo</title><style>
:root{--bg:#0b0e12;--card:#141a21;--line:#232c36;--tx:#e8eef5;--mut:#8a97a6;--ok:#06d6a0;--bad:#ef476f;--acc:#4cc9f0}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--tx);font:16px/1.4 -apple-system,Segoe UI,Roboto,sans-serif;padding:24px}
h1{font-size:15px;letter-spacing:.12em;text-transform:uppercase;color:var(--mut);margin:0 0 4px}#status{color:var(--acc);margin-bottom:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:18px}
.lab{color:var(--mut);font-size:13px;text-transform:uppercase;letter-spacing:.08em}.big{font-size:56px;font-weight:700;line-height:1.1;margin-top:6px}.mid{font-size:30px;font-weight:600;margin-top:10px}
.bar{height:10px;background:var(--line);border-radius:5px;margin-top:14px;overflow:hidden}.bar i{display:block;height:100%;background:var(--acc);transition:width .4s}
.win{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}.w{padding:10px;border-radius:8px;border:1px solid var(--line);text-align:center;transition:all .4s}.w.open{background:#0b3a4a;border-color:var(--acc)}
table{width:100%;border-collapse:collapse;margin-top:8px;font-size:14px}td{padding:6px 8px;border-top:1px solid var(--line);vertical-align:top}.ok{color:var(--ok)}.bad{color:var(--bad)}.m{color:var(--mut)}
.flash{animation:f .8s}@keyframes f{from{background:#1d3a35}to{background:var(--card)}}
</style></head><body><h1>QNX 8.0 in-car voice commands</h1><div id="status">connecting</div>
<div class="grid"><div class="card" id="c_temp"><div class="lab">Cabin temperature</div><div class="big" id="temp">-</div></div>
<div class="card" id="c_nav"><div class="lab">Navigation</div><div class="mid" id="nav">-</div></div>
<div class="card" id="c_vol"><div class="lab">Volume</div><div class="mid" id="vol">-</div><div class="bar"><i id="volbar"></i></div></div>
<div class="card" id="c_call"><div class="lab">Phone</div><div class="mid" id="call">idle</div></div>
<div class="card" id="c_win"><div class="lab">Windows</div><div class="win" id="win"></div></div></div>
<div class="card" style="margin-top:16px"><div class="lab">What it heard</div><table id="log"></table></div>
<script>
const $=id=>document.getElementById(id);let prev={};
function flash(id){const e=$(id);e.classList.remove('flash');void e.offsetWidth;e.classList.add('flash')}
function cap(s){return s?s.replace(/_/g,' ').replace(/^./,c=>c.toUpperCase()):s}
function render(s){$('status').textContent=s.status;
 $('temp').textContent=s.temp+' °C';$('nav').textContent=s.nav?'To '+cap(s.nav):'No destination';
 $('vol').textContent=s.muted?'Muted':s.volume+' / 10';$('volbar').style.width=(s.muted?0:s.volume*10)+'%';
 $('call').textContent=s.call?'Calling '+cap(s.call):'Idle';
 $('win').innerHTML=Object.entries(s.windows).map(([k,v])=>'<div class="w '+(v==='open'?'open':'')+'">'+cap(k)+'<br><b>'+v+'</b></div>').join('');
 $('log').innerHTML=s.log.map(l=>'<tr><td class="m">'+l.t+'</td><td>'+esc(l.heard)+'</td><td class="'+(l.ok?'ok':'bad')+'">'+esc(l.what)+'</td><td class="m">'+(l.ms/1000).toFixed(1)+' s</td></tr>').join('');
 const k={c_temp:s.temp,c_nav:s.nav,c_vol:s.volume+':'+s.muted,c_call:s.call,c_win:JSON.stringify(s.windows)};
 for(const id in k){if(prev[id]!==undefined&&prev[id]!==k[id])flash(id);prev[id]=k[id]}}
function esc(t){return String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
const es=new EventSource('/events');es.onmessage=m=>render(JSON.parse(m.data));es.onerror=()=>$('status').textContent='dashboard connection lost';
</script></body></html>"""

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--port", type=int, default=8090); ap.add_argument("--tee")
    a = ap.parse_args()
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
    print("[dashboard] http://127.0.0.1:%d  (reading voicecmd events on stdin)" % a.port, file=sys.stderr, flush=True)
    tee = open(a.tee, "a") if a.tee else None
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"): continue
        if tee: tee.write(line + "\n"); tee.flush()
        print(line, flush=True)
        try: ev = json.loads(line)
        except ValueError: continue
        with CV:
            apply(ev); CV.notify_all()
    with CV: STATE["status"] = "audio stream ended"; STATE["v"] += 1; CV.notify_all()
    print("[dashboard] stream ended; final state stays up, Ctrl-C to quit", file=sys.stderr, flush=True)
    try: threading.Event().wait()
    except KeyboardInterrupt: pass

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: sys.exit(0)
