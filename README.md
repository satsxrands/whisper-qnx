# whisper-qnx

Offline voice commands on QNX 8.0: whisper.cpp (ggml) cross-compiled for QNX, with audio capture and a command layer.
Upstream: https://github.com/ggml-org/whisper.cpp (referenced by URL and a pinned commit in WHISPER_REF; not a fork).
Hackathon: https://qnx.devpost.com/ . Licence: Apache-2.0.

STATUS (2026-11-21): whisper.cpp v1.9.5 cross-builds for QNX 8.0 (aarch64le and x86_64) and transcribes on a QNX 8.0 x86_64 VM.

## Build
SDP 8.0 inside the licence-pinned Linux container (see scripts/qnx-container.sh; never change hostname/MAC).
`scripts/build.sh aarch64le|x86_64` clones upstream whisper.cpp to ~/qnx/work, checks out WHISPER_REF, applies
patches/0001-qnx-memory-info.patch (QNX has no _SC_PHYS_PAGES) and builds libwhisper + whisper-cli with
cmake/qnx800.toolchain.cmake (shared libs, GGML_NATIVE/LTO/OPENMP/VULKAN off). Notes: the container must run as
root with HOME=/root (licence files), and CMAKE_SYSTEM_PROCESSOR is mapped aarch64le to aarch64 so ggml picks the ARM path.

## Target
`scripts/make-image.sh` builds a QNX 8.0 x86_64 QEMU image (mkqnximage, 1 GB data partition for the model).
`scripts/run-vm-x86_64.sh &` boots it headless (TCG, ssh on localhost:2222). aarch64le images are blocked: mkqnximage
needs startup-qemu-virt from the QEMU quickstart BSP, installable only via the QNX Software Center.

## Quickstart image (2026-11-21)
The QNX quickstart package (com.qnx.qnx800.quickstart.qemu, 20260902) is an **x86_64** image only: no aarch64 image, so no hvf
on Apple Silicon (startup-qemu-virt is still absent from the SDP). Unpack: `mkdir -p ~/qnx/work/quickstart && cd ~/qnx/work/quickstart &&
cat ~/qnx/qnx800/images/qemu/*.tar.gz.* | tar xzf -`, then `qemu-img convert -O qcow2 qemu/output/disk-qemu qemu/output/disk.qcow2`
and delete the 47 GB raw disk (the qcow2 is 5 GB). Boot: `scripts/run-vm-quickstart.sh &` (ssh localhost:2223 as qnxuser; root ssh is refused).
Serial console is tcp 4555 (login root / root, image defaults); use it once to install `~/qnx/vm_key.pub` into /data/home/qnxuser/.ssh/authorized_keys.
Use `VM=quickstart scripts/vm-ssh.sh` and `scripts/transcribe-on-target.sh`. Build with live audio support: `SND=1 scripts/build-voicecmd.sh x86_64`.
Audio drivers in the SDP: x86_64 has sndrv-ctrl-intel_hda and sndrv-ctrl-usb; aarch64le has ONLY usb (no intel_hda, no virtio-snd).
Status: io-snd + intel_hda reach the controller (needs libpci.so + libpci_hw_common.so from the SDP and PCI_CAP_MODULE_DIR=/proc/boot,
QEMU `-device intel-hda,msi=on`) but QNX sees STATESTS=0 (no codec) so no /dev/snd yet. USB audio: QEMU qemu-xhci fails 'Enable Slot Command' in devu-hcd-xhci.

## Run
`scripts/transcribe-on-target.sh` copies whisper-cli, libs, ggml-tiny.en and jfk.wav to /data/w and runs
`LD_LIBRARY_PATH=lib ./bin/whisper-cli -m ggml-tiny.en.bin -f jfk.wav -t 4`.

## Results
jfk.wav (11.0 s), tiny.en, 4 threads, QEMU TCG on Apple Silicon (software emulation, generic CPU kernels):
"And so my fellow Americans ask not what your country can do for you / ask what you can do for your country."
Total 88.9 s (model load 6.3 s), real-time factor about 8.1 (emulated; not representative of real hardware).

## Live demo (no hardware): Mac microphone -> QNX VM -> dashboard (2026-11-21)
The VM is the QNX 8.0 x86_64 quickstart image under QEMU TCG. Audio never touches the guest sound stack: the Mac streams 16 kHz mono s16le PCM
over TCP (`scripts/mac-mic-stream.sh`, ffmpeg avfoundation), `voicecmd -a tcp:10.0.2.2:5555` inside the VM reads it, and its JSON intents are piped
back over ssh into `demo/dashboard.py` (cabin temperature, navigation, volume, phone, windows) at http://127.0.0.1:8090.
```
scripts/build.sh x86_64 && scripts/build-voicecmd.sh x86_64     # once
(scripts/run-vm-quickstart.sh > /dev/null 2>&1 &); sleep 90      # boot the VM (SMP=4 default)
scripts/stage-vm.sh                                              # once per rebuild
scripts/mac-mic-stream.sh --list                                 # optional: audio inputs (default input is used unless MIC=<index>)
scripts/demo-live.sh                                             # live mic; or: scripts/demo-live.sh --wav some.wav
```
First live run: macOS asks Terminal for microphone access. Speak after the dashboard shows "listening". Expect about 8 s from end of
phrase to dashboard update (emulation, not hardware).

## Latency tuning (quickstart VM, 4 vCPU TCG, tiny.en, 20 phrases, file input, per command from end of speech to JSON)
| Config | Mean | Min / Max | Accuracy |
|---|---|---|---|
| Before (full vocab prompt, audio_ctx margin 50 frames, max 16 tokens) | 10.6 s | 9.8 / 12.2 | 20/20 |
| Tuned default (5-phrase prompt, margin 25, min ctx 80, max 10 tokens) | 7.9 s | 6.8 / 9.5 | 20/20 (clean voice A), 20/20 (voice C) |
Findings: (1) the vocabulary prompt costs about 0.15 s per token per command in emulation, the biggest cheap win (no prompt: 4.8 to 6.2 s but
accuracy drops, "Call Sarah" heard as "Call sir", "Volume up" as "Volume Upper"); (2) encoder time scales with audio_ctx; (3) AVX2/FMA ggml build
(`SIMD=avx2`) is SLOWER under TCG (17 s vs 11.5 s per command): leave it off; (4) 3 threads slower than 4; the Mac has 4 performance cores, so
more vCPUs were not tried beyond 4. Trade-off: with 10 dB cabin noise on a second voice the tuned build scores 18/20 (misses: "Mute" garbled,
"Call Zebediah" heard as "Call Sarah", a wrong action caused by the prompt bias; a smoke test with TTS voices, not a benchmark).
VAD: the noise floor now calibrates on the first 300 ms and rises slowly while a gate is open, margin 10 dB. Before, constant noise above -45 dBFS held the
gate open until the 8 s cap (0 utterances found on the noisy streams); now it segments all 20 (`voicecmd/tests/vad_segments.c`).
Tunables: `voicecmd --ctx-margin N --audio-ctx-min N --max-tokens N --hang-ms N --prompt TEXT --timings`; benchmark with `scripts/bench-vm.sh`.
End to end proof: a recorded WAV played as the mic over TCP into the VM: 20/20, mean 10.7 s (before tuning), `voicecmd/results/mic_wav_over_tcp_quickstart.jsonl`.

## Real QNX OS 8.0 hardware path: AWS Graviton (2026-11-26)
Same binaries, cross-built with `scripts/build.sh aarch64le && scripts/build-voicecmd.sh aarch64le`, run on the QNX OS 8.0.5 arm64 AMI
(AWS Marketplace, c7g.xlarge = 4 Graviton3 vCPU, login user `qnxuser`). `scripts/stage-aws.sh` pushes the files, `scripts/bench-aws.sh <streamX.wav> <label> -t 4`
runs the 20-phrase benchmark, `scripts/demo-live-aws.sh [--wav x.wav]` streams the Mac mic over an ssh reverse tunnel into voicecmd on the instance.
Per command, end of speech to JSON, tiny.en, 4 threads, 20 phrases each (TTS voices, smoke test not a benchmark):
| Set | QEMU TCG (x86_64) | AWS Graviton3 (QNX 8.0, aarch64le) |
|---|---|---|
| Voice A clean | 7.9 s mean (6.8 / 9.5), 20/20 | 128 ms mean (112 / 159), 20/20 |
| Voice B 10 dB noise | | 119 ms mean (85 / 141), 19/20 |
| Voice C clean | 7.9 s mean (6.8 / 9.7), 20/20 | 125 ms mean (109 / 143), 20/20 |
| Voice D 10 dB noise | 7.8 s mean (6.3 / 9.6), 18/20 | 111 ms mean (89 / 134), 18/20 |
Threads on set A: 1 thread 423 ms, 2 threads 228 ms, 4 threads 128 ms. whisper-cli tiny.en on jfk.wav (11 s): 1.76 s total (QEMU 77 s). Raw logs: `voicecmd/results/aws/`.
