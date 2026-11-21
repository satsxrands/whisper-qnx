#!/usr/bin/env python3
"""Score voicecmd JSON-lines output against data/commands.tsv. usage: score.py commands.tsv run.jsonl"""
import sys, json
exp = [l.rstrip("\n").split("\t") for l in open(sys.argv[1]) if l.strip()]
ev = [json.loads(l) for l in open(sys.argv[2]) if l.startswith("{")]
res = [e for e in ev if e["event"] in ("command", "rejected")]
ok = 0; lat = []
for i, e in enumerate(exp):
    id_, said, intent, slots = e
    r = res[i] if i < len(res) else None
    if r is None: print(id_, "MISSING", said); continue
    if intent == "REJECT":
        good = r["event"] == "rejected"  # any rejection is a safe outcome; reason mismatch is noted below
    else:
        good = r["event"] == "command" and r["intent"] == intent and json.dumps(r["slots"], separators=(",", ":")) == slots
    note = " (reason %s, wanted %s)" % (r["reason"], slots) if intent == "REJECT" and good and r["reason"] != slots else ""
    ok += good; lat.append(r["latency_ms"])
    print(id_, "OK  " if good else "FAIL", repr(said), "->", repr(r["transcript"][:70]), r.get("intent") or r.get("reason"), r["latency_ms"], "ms" + note)
extra = len(res) - len(exp)
print(f"accuracy {ok}/{len(exp)}  utterances detected {len(res)} (expected {len(exp)}, extra {extra})")
if lat: print(f"latency ms: mean {sum(lat)/len(lat):.0f} min {min(lat)} max {max(lat)}")
