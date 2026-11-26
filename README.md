# whisper-qnx

**Offline, hands-free in-car voice commands on QNX OS 8.0: live audio streamed into QNX, whisper.cpp speech recognition, a safety-first command grammar, and 110 to 145 ms from end of speech to action on real QNX 8.0 (AWS Graviton).**

Category: Automotive & Mobility, QNX Global Hackathon (https://qnx.devpost.com/). Licence: Apache-2.0.

Demo video: _link added at submission_. Demo GIF / screenshot of the dashboard: _placeholder, to be added with the video (none committed yet)_.

## What is new here, and what is not

whisper.cpp itself is **already ported to QNX by QNX**: https://github.com/qnx-ports/aports/tree/master/extra/whisper.cpp (CPU backend, merged August 2026). Upstream project: https://github.com/ggml-org/whisper.cpp. This repository does not claim that port, and is not a fork: it pins an upstream commit by URL in `WHISPER_REF` and applies one small patch.

What this repository adds on top of that:

| Added here | Where |
|---|---|
| A live audio pipeline into QNX: 16 kHz mono PCM streamed over TCP (an ssh reverse tunnel on AWS), one audio-source interface with `file:`, `tcp:` and an `snd:` (QNX io-snd / ALSA) backend | `voicecmd/src/audio_source.c` |
| Energy VAD tuned for cabin noise (adaptive noise floor, 600 ms hangover, 200 ms preroll) so whisper only runs on speech | `voicecmd/src/vad.h` |
| A fixed in-car command grammar after whisper: 12 intents, slot checking (temperature range, known contacts and destinations), near-miss word repair, and typed rejection of everything else | `voicecmd/src/grammar.cpp` |
| Decoder prompt bias plus audio-context sizing, so a 1 to 3 s command costs about 110 to 160 ms on Graviton | `voicecmd/src/voicecmd.cpp` |
| A live in-car dashboard (cabin temperature, navigation, volume, mute, phone, windows, media) driven by the JSON events | `demo/dashboard.py` |
| A repeatable QNX 8.0 cross-build in a container, for x86_64 and aarch64le, plus a small patch for ggml's memory query (QNX has no `_SC_PHYS_PAGES`) | `scripts/`, `cmake/`, `patches/` |
| Measured results on real QNX OS 8.0 (AWS Graviton3) and on QEMU, with raw logs | `voicecmd/results/` |
| A step-by-step codelab in the qnx/codelabs format | `codelab/whisper-voice-commands-qnx.md` |

The recogniser is the stock whisper `tiny.en` model. The grammar is a post-recognition layer, not whisper's GBNF grammar (tiny.en token boundaries make a token grammar fragile).

## Architecture

```
 Mac / any host                           QNX OS 8.0 target (AWS Graviton3 aarch64le, or QEMU x86_64)
+--------------------+  16 kHz mono     +----------------------------------------------------------+
| microphone         |  s16le PCM       | voicecmd                                                 |
| ffmpeg (avfoundation) --- TCP ------->|  audio_source (tcp: | file: | snd:)                      |
| or --wav file      |  (ssh -R tunnel  |        |                                                 |
+--------------------+   on AWS)        |        v                                                 |
                                        |  energy VAD  (20 ms frames, adaptive floor, 600 ms hang) |
                                        |        v  one utterance                                  |
                                        |  whisper.cpp tiny.en, greedy, vocabulary prompt,         |
                                        |  audio_ctx sized to the utterance, max 10 tokens         |
                                        |        v  transcript                                     |
                                        |  grammar: normalise, number words, near-miss repair,     |
                                        |  slot + range check, ambiguity reject                    |
                                        |        v                                                 |
                                        |  one JSON line per utterance on stdout                   |
                                        +-------------------------|--------------------------------+
                                                                  | ssh (stdout)
                                                                  v
                                        demo/dashboard.py on the host, http://127.0.0.1:8090
                                        (cabin state + live log; in a real vehicle this is where the
                                         head unit / vehicle bus would consume the intents)
```

Event format, one line per utterance:
`{"event":"command","intent":"call","slots":{"contact":"sarah"},"match":"exact","transcript":"Call Sarah.","audio_ms":1500,"latency_ms":110}`
or `{"event":"rejected","reason":"no_match",...}`. Other events: `ready`, `eof`, `error`.
Latency is measured from the end of speech (VAD closes the gate) to the JSON line.

## Supported commands

| Intent | Examples | Slots |
|---|---|---|
| `set_temperature` | "Set temperature to 21", "Set the temperature to twenty four degrees" | `celsius`, 16 to 30 only |
| `navigate` | "Navigate home", "Navigate to the airport" | `destination`: home, work (alias office), airport, petrol_station (aliases fuel, gas), gym |
| `call` | "Call Mom", "Call Sarah" | `contact`: mom (mum, mother), dad (father), john, sarah, emma, office |
| `volume_up`, `volume_down` | "Volume up", "Turn the volume up" | none |
| `mute` | "Mute" | none (the dashboard toggles) |
| `open_window`, `close_window` | "Open the window", "Open the passenger window" | `which`: all (default), driver, passenger, rear, front |
| `play_media` | "Can you play Spotify", "Play music", "Play the radio", "Play a podcast" | `source`: spotify, music, radio, podcast |
| `pause_media` | "Pause", "Stop music" | none |
| `next_track` | "Next track", "Skip" | none |
| `previous_track` | "Previous track" | none |

Rejected on purpose, with a typed reason: out-of-range temperature (`out_of_range`), unknown contact, destination or media source (`unknown_contact`, `unknown_destination`, `unknown_source`), unsupported targets ("Open the door"), bare words such as "play", "next", "stop", and anything off topic ("What is the weather like today") (`no_match`). The grammar has 78 unit cases (clean, noisy near-misses, out of range, unknown, ambiguous, injection-style text); they pass on the host and on the QNX instance (`voicecmd/tests/test_grammar.cpp`).

## Results

All numbers are recorded in `voicecmd/results/` (raw JSON event logs) and scored with `voicecmd/data/score.py`. Latency is per command, end of speech to JSON, `tiny.en`, 4 threads, 20 phrases per set.

### Real QNX OS 8.0 on AWS Graviton3 vs QEMU

| Set | QEMU TCG (x86_64 quickstart image) | AWS Graviton3 (QNX OS 8.0.5, aarch64le) |
|---|---|---|
| Voice A, clean | 7.9 s mean (6.8 / 9.5), 20/20 | 128 ms mean (112 / 159), 20/20 |
| Voice B, 10 dB noise | not run at the tuned config | 119 ms mean (85 / 141), 19/20 |
| Voice C, clean | 7.9 s mean (6.8 / 9.7), 20/20 | 125 ms mean (109 / 143), 20/20 |
| Voice D, 10 dB noise | 7.8 s mean (6.3 / 9.6), 18/20 | 111 ms mean (89 / 134), 18/20 |

About 60x faster on real hardware, accuracy identical to QEMU. Threads on set A (Graviton): 1 thread 423 ms, 2 threads 228 ms, 4 threads 128 ms. `whisper-cli` tiny.en on the 11 s `jfk.wav`: 1.76 s total on Graviton, 77 s on QEMU. Model load 0.47 s on Graviton.

### Current default prompt (8 phrases, includes the media commands), Graviton

| Set | Accuracy | Mean latency |
|---|---|---|
| A clean | 20/20 | 144 ms |
| B 10 dB noise | 19/20 | 137 ms |
| C clean | 20/20 | 142 ms |
| D 10 dB noise | 18/20 | 130 ms |

The 5-phrase prompt used in the first table is about 10 to 15 ms faster per command with the same accuracy; the longer prompt exists to cover the media commands. Media phrases (12 each, macOS `say`): voice Samantha 12/12 correct action (121 to 152 ms); voice Daniel 10/12 ("Pause" heard as "Cause.", "Skip" as "Skype.", both rejected, no wrong action).

### Live microphone on the real instance (2026-11-26)

A person speaking into the Mac microphone, streamed to the Graviton instance: 7 of 7 car commands correct (Call Sarah, Volume up, Mute, Call Mom, at 110 to 120 ms), 5 of 5 media commands correct on the second attempt after the grammar fix described in the limitations ("Can you play Spotify", "Play music", "Next track", "Pause", "Skip", at 108 to 142 ms), and every non-command (weather questions and similar) rejected. Raw events: `voicecmd/results/aws/live_mic_2026-11-26.jsonl`. This is a handful of utterances from one speaker, a demonstration and not a benchmark.

## Quickstart

Both paths use the same sources and the same container-based cross-build. QNX version: **QNX SDP 8.0** (baseline 8.0.0.00141T202311271501L). Toolchain: `qcc` / `q++` from SDP 8.0 (`-Vgcc_ntox86_64_cxx` and `-Vgcc_ntoaarch64le_cxx`), CMake 3.22, Ninja, in an `ubuntu:20.04` linux/amd64 container (QNX host tools run on Linux x86_64). Upstream whisper.cpp: v1.9.5, commit pinned in `WHISPER_REF`. The full newcomer walkthrough (SDP install, container, licence) is the codelab: [`codelab/whisper-voice-commands-qnx.md`](codelab/whisper-voice-commands-qnx.md).

The scripts expect this layout (outside git): SDP in `~/qnx/qnx800`, the licence-bearing root home in `~/qnx/dotroot`, models in `~/qnx/models` (`ggml-tiny.en.bin`, `jfk.wav`), build output in `~/qnx/work`. The build container must run with a fixed hostname and MAC address (`scripts/qnx-container.sh` does this) because the SDP licence activation is bound to the host identity.

### Common: build once

```
scripts/docker-image.sh                      # builds the "qnx800" image (contains no QNX software)
scripts/build.sh x86_64                      # whisper.cpp for QEMU
scripts/build.sh aarch64le                   # whisper.cpp for Graviton
scripts/build-voicecmd.sh x86_64             # voicecmd + grammar test, same for aarch64le
scripts/build-voicecmd.sh aarch64le
# binaries: ~/qnx/work/build-<arch>/bin (whisper-cli, libwhisper, libggml*) and ~/qnx/work/vc-<arch>/voicecmd
```

### Path A: QEMU x86_64 (no hardware, slow: emulation)

Target: QNX 8.0 x86_64 quickstart image (`com.qnx.qnx800.quickstart.qemu`) booted under QEMU with TCG. Unpack, boot and ssh details are in [`docs/DEVLOG.md`](docs/DEVLOG.md) ("Quickstart image") and the codelab.

```
(scripts/run-vm-quickstart.sh > /dev/null 2>&1 &); sleep 90     # boot, ssh on localhost:2223 as qnxuser
scripts/stage-vm.sh                                             # push voicecmd, libs, model
scripts/demo-live.sh                                            # live mic; or: scripts/demo-live.sh --wav some.wav
# dashboard: http://127.0.0.1:8090   (expect about 8 s from end of phrase to dashboard: emulation)
```

### Path B: AWS Graviton, QNX OS 8.0 (real hardware, about 130 ms)

Target: the QNX OS 8.0 arm64 AMI from AWS Marketplace on a `c7g.xlarge` (4 Graviton3 vCPU), login user `qnxuser`, SSH key pair of your own. Allow SSH (port 22) from your own IP only. Put the instance public IP in `~/qnx/aws/ip` and the private key at `~/qnx/aws/qnx-hackathon.pem` (or export `AWS_QNX_IP` and `AWS_QNX_KEY`).

```
scripts/aws-ssh.sh 'echo ok'                          # connectivity
scripts/stage-aws.sh                                  # push voicecmd, libs, model, test streams to ~/x on the instance
scripts/bench-aws.sh streamA.wav aws -t 4             # 20-phrase benchmark, prints accuracy and latency
PORT=5575 scripts/demo-live-aws.sh                    # live Mac microphone -> ssh -R tunnel -> voicecmd on QNX
PORT=5575 scripts/demo-live-aws.sh --wav x.wav        # a WAV played as the microphone
# dashboard: http://127.0.0.1:8090   (macOS asks Terminal for microphone access on first live run)
```

Use a free local port for `PORT` (5555 was already taken by another tool on the dev machine). Stop the instance when finished: it bills while running (the 2026-11-26 session of about 0.2 h cost about $0.12 in EC2 plus QNX software charges).

Test streams are made with macOS `say -v <voice> -o c01.wav --data-format=LEI16@16000 "..."`, joined by `voicecmd/data/make_stream.py`, with cabin noise added by `voicecmd/data/add_noise.py <in> <out> 10`; the 20 expected results are in `voicecmd/data/commands.tsv`.

## Proof it runs on QNX 8.0

- The binaries are cross-built with the SDP 8.0 `qcc` (a smoke-test program built the same way is a QNX ELF executable with interpreter `/usr/lib/ldqnx-64.so.2`); they were run on QNX OS 8.0 on x86_64 under QEMU and on aarch64le on AWS Graviton.
- Raw event logs of every result above: `voicecmd/results/` and `voicecmd/results/aws/`.
- The grammar test (`test_grammar`) passes on the QNX instance as well as the host.
- Live run on the real instance: `voicecmd/results/aws/live_mic_2026-11-26.jsonl` and the demo video.

## Limitations, stated plainly

- **Benchmarks use synthetic speech.** Voices are macOS `say` text-to-speech (Samantha, Daniel and others), 20 phrases per set including 5 that must be rejected, noise is synthetic (low-passed noise plus 120 Hz hum at 10 dB). This is a smoke test, not a benchmark of real drivers, accents or real cabin acoustics. The live microphone check is one speaker and a few dozen utterances.
- **Known misses.** Under noise "Mute" was heard as "New." (rejected, no action). "Call Zebediah" under noise was heard as "Call Sarah": a wrong action caused by the vocabulary prompt bias, the one failure that matters; it is why a shipped system would confirm calls and payments. "Pause" and "Skip" with one TTS voice became "Cause." and "Skype." (rejected). `tiny.en` hears "play" as "please" before a source word ("Can you please Spotify?"); the grammar now treats a lone source word as `play_media` (marked `fuzzy`), a deliberate trade-off. `base.en` was worse than `tiny.en` on a host-only set (19/20 twice) because of prompt bias.
- **No physical board.** "Real hardware" here means QNX OS 8.0 on an AWS Graviton3 cloud instance, not an automotive SoC.
- **Audio does not come from a QNX microphone driver.** The SDP in this setup ships the audio packages only through the QNX Software Center; QEMU's emulated Intel HDA showed no codec to QNX and USB audio failed in `devu-hcd-xhci`. So audio is streamed into QNX over the network. The native `snd:` backend (libasound) compiles with `SND=1 scripts/build-voicecmd.sh x86_64` but has not been run.
- **Intents are not actuated.** The dashboard simulates a cabin; nothing is wired to a vehicle bus.
- English only, `tiny.en` only, energy-based VAD (no neural VAD), fixed vocabulary of contacts and destinations.
- The QEMU path is software emulation (about 8 s per command, real-time factor about 4 to 8), not representative of hardware.

## Repository map

`voicecmd/` daemon, grammar, tests, data and results · `demo/` dashboard · `scripts/` build, stage, benchmark and demo scripts · `cmake/` QNX toolchain file · `patches/` one ggml patch · `docker/qnx-build/` build image (no QNX software inside) · `codelab/` tutorial · `docs/DEVLOG.md` chronological dev log.

## Licence

Apache-2.0, see `LICENSE`. whisper.cpp and ggml are MIT licensed (https://github.com/ggml-org/whisper.cpp) and are fetched from upstream at build time, not vendored. The QNX SDP is not included and is used under your own QNX licence (free non-commercial licence: https://www.qnx.com/products/everywhere/).
