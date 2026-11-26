id: whisper-voice-commands-qnx
title: Offline Voice Commands on QNX 8.0 with whisper.cpp
summary: Cross-build whisper.cpp and a voice-command daemon for QNX 8.0, stream live microphone audio into a QNX target, and drive an in-car dashboard from spoken commands, on QEMU or on a real QNX OS 8.0 AWS Graviton instance.
categories: AI & ML
tags: intermediate
platforms: QEMU, AWS Graviton
published: 2026-11-26
featured: false
status: draft
authors: satsxrands
feedback_link: https://github.com/qnx/codelabs/issues

# Offline Voice Commands on QNX 8.0 with whisper.cpp

## Overview
Duration: 5:00

In this codelab you build a small offline voice-command system that runs on QNX 8.0. A microphone stream goes into a QNX target, a voice-activity detector finds each spoken command, whisper.cpp transcribes it, and a fixed in-car grammar turns the text into a structured command such as `{"intent":"call","slots":{"contact":"sarah"}}`. A browser dashboard shows a simulated cabin reacting to the commands.

whisper.cpp itself is already packaged for QNX in [qnx-ports/aports](https://github.com/qnx-ports/aports/tree/master/extra/whisper.cpp). This codelab starts from the upstream project, [ggml-org/whisper.cpp](https://github.com/ggml-org/whisper.cpp), and shows what you add on top: live audio into QNX, speech segmentation, a command grammar that rejects anything it does not understand, and a measured latency story on real QNX OS 8.0 hardware (an AWS Graviton instance) as well as on QEMU.

The complete project is at https://github.com/satsxrands/whisper-qnx (Apache-2.0).

**What you will learn:**

* How to set up a QNX SDP 8.0 cross-build environment in a container, on macOS or Linux
* How to cross-build whisper.cpp and a C/C++ daemon for QNX (`x86_64` and `aarch64le`) with a CMake toolchain file
* How to choose a target (QEMU or a real QNX OS 8.0 AWS Graviton instance) and stage files onto it
* How to test the recogniser on a WAV file, then with a live microphone
* How the grammar layer works and how to add a command of your own

### What you'll need

- A free non-commercial QNX licence and QNX SDP 8.0 (see the [Get a Free Non-commercial QNX License](https://github.com/qnx/codelabs/tree/main/markdown/get-non-commercial-qnx-license) codelab)
- A macOS or Linux host with Docker (Rancher Desktop or Docker Desktop on macOS), about 15 GB of free disk, `git`, `ffmpeg` and `python3`
- For the QEMU path: `qemu-system-x86_64` (`brew install qemu` on macOS, your distribution's `qemu-system-x86` package on Linux). It is software emulation, so commands take several seconds each
- For the real-hardware path: an AWS account, with a small budget (the validated session cost about $0.12 for about 12 minutes; stop the instance when done)
- A microphone on the host for the live step (macOS asks Terminal for microphone permission the first time)

> **What this does not do:** audio reaches QNX as a network stream of PCM samples (the audio capture is on the host). A native QNX microphone path (`snd:` backend using io-snd and libasound) is in the code and compiles, but it has not been run, because the QEMU image offered no working audio codec. See Limitations at the end.

## Install QNX SDP 8.0
Duration: 20:00

QNX host tools run on Linux and Windows x86_64 only, so on macOS (and on Arm hosts) you cross-build inside an x86_64 Linux container. The SDP itself is installed once into `~/qnx/qnx800`.

1. Get a myQNX account and a non-commercial licence: follow the [licence codelab](https://github.com/qnx/codelabs/tree/main/markdown/get-non-commercial-qnx-license) until the licence is deployed to your myQNX user.
2. Install **QNX SDP 8.0** with the [QNX Software Center](https://www.qnx.com/download/group.html?programid=29178), destination `~/qnx/qnx800`. On macOS, QNX documents the container route here: https://gitlab.com/qnx/projects/docker-mac and the blog post "How to use macOS to develop for QNX 8.0".
3. Create the folders the scripts expect:

```bash
mkdir -p ~/qnx/dotroot ~/qnx/models ~/qnx/work
ls ~/qnx/qnx800/qnxsdp-env.sh
```

> **Why `dotroot`?** The build container runs as `root` with `HOME=/root`, and `~/qnx/dotroot` is mounted there. The QNX licence files must be found under it (`~/qnx/dotroot/.qnx/license`). The QNX licence activation is bound to the machine identity, so the container is always started with the same fixed `--hostname qnx-dev --mac-address 02:42:ac:11:00:42`. Do not change those two values after you activate.

### Headless install (what the author used)

On macOS the Software Center GUI is not available, so the author ran the Software Center command-line client (`qnxsoftwarecenter_clt`, from the Linux installer `qnx-setup-*-linux.run`, unpacked inside an `ubuntu:20.04` `linux/amd64` container started with the fixed hostname and MAC above, with `~/qnx` mounted at `/qnx` and `~/qnx/dotroot` at `/root`). The options that worked:

```bash
C="./qnxsoftwarecenter_clt -myqnx.user <your-myqnx-email> -myqnx.password <password>"
$C -syncLicenseKeys
$C -listLicenseKeys                         # shows your key
$C -activateLicenseKey <key>                # binds it to the fixed container identity
$C -mirror
$C -listAccessible | grep '^com.qnx.qnx800='
$C -installBaseline com.qnx.qnx800/8.0.0.00141T202311271501L -destination /qnx/qnx800
```

The password is only accepted as an argument; type it in your own terminal and never commit it. The SDP is about 3.6 GB on disk. If your licence lists a newer baseline, use the id printed by `-listAccessible`.

## Build the container image
Duration: 5:00

Clone the project and build the build image. The image contains no QNX software: it only has Ubuntu 20.04, compilers for the helper tools, Ninja and CMake 3.22 (Ubuntu 20.04 ships 3.16, too old for whisper.cpp). The SDP is mounted from your home directory at run time.

```bash
git clone https://github.com/satsxrands/whisper-qnx.git
cd whisper-qnx
scripts/docker-image.sh
```

> **macOS with Rancher Desktop:** the scripts put `~/.rd/bin` on `PATH`. If you use Docker Desktop, `docker` is already on `PATH`. On Apple Silicon the image runs under emulation (`--platform linux/amd64`), which makes the first build slower.

Check that the SDP and licence work inside the container by compiling a tiny program for both architectures:

```bash
cat > ~/qnx/work/hello.c <<'EOF'
#include <stdio.h>
int main(void) { puts("hello from QNX"); return 0; }
EOF
scripts/qnx-container.sh 'cd /qnx/work && qcc -Vgcc_ntox86_64 hello.c -o hello_x86_64 && qcc -Vgcc_ntoaarch64le hello.c -o hello_aarch64le && file hello_*'
```

You should see two ELF executables. If `qcc` complains about the licence, see Troubleshooting.

## Get the model and cross-build
Duration: 15:00

Download the `tiny.en` model (English only) from the upstream whisper.cpp model repository and keep a sample WAV for the first test:

```bash
curl -L -o ~/qnx/models/ggml-tiny.en.bin \
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin
```

Build whisper.cpp for QNX. `scripts/build.sh` clones upstream into `~/qnx/work/whisper.cpp`, checks out the commit pinned in `WHISPER_REF` (v1.9.5), applies `patches/0001-qnx-memory-info.patch`, and runs CMake with `cmake/qnx800.toolchain.cmake`:

```bash
scripts/build.sh x86_64        # for the QEMU target
scripts/build.sh aarch64le     # for the AWS Graviton target
cp ~/qnx/work/whisper.cpp/samples/jfk.wav ~/qnx/models/jfk.wav
```

The toolchain file selects `qcc`/`q++` with `-Vgcc_ntox86_64_cxx` or `-Vgcc_ntoaarch64le_cxx`. The flags that matter are `GGML_NATIVE=OFF`, `GGML_LTO=OFF`, `GGML_OPENMP=OFF`, `GGML_VULKAN=OFF`, `WHISPER_SDL2=OFF` and shared libraries on. The one source patch exists because QNX has no `_SC_PHYS_PAGES`: ggml's CPU backend asks for total memory, so the patch sums the `ram` entries of the system page instead. Also note that `CMAKE_SYSTEM_PROCESSOR` is mapped from `aarch64le` to `aarch64` so ggml picks its Arm code path.

Now build the voice-command daemon and its grammar test against those libraries:

```bash
scripts/build-voicecmd.sh x86_64
scripts/build-voicecmd.sh aarch64le
ls ~/qnx/work/vc-x86_64 ~/qnx/work/vc-aarch64le          # voicecmd, test_grammar
```

Binaries for each architecture end up in `~/qnx/work/build-<arch>/bin` (`whisper-cli`, `libwhisper*.so`, `libggml*.so`) and `~/qnx/work/vc-<arch>/voicecmd`.

## Choose a target
Duration: 3:00

| | QEMU x86_64 | AWS Graviton, QNX OS 8.0 |
|---|---|---|
| What it is | QNX 8.0 quickstart image under software emulation | Real QNX OS 8.0.5 arm64 AMI on a `c7g.xlarge` (4 Graviton3 vCPU) |
| Cost | Free | A few cents per session (billed per hour while running) |
| Speed (tiny.en, per command) | about 8 s | about 110 to 145 ms |
| Use it for | Offline, free, correctness | Real-time behaviour, the demo |

You can do both. Everything after this step is shown for both targets.

### Target A: QEMU x86_64

Unpack the QNX quickstart QEMU image. It is the package `com.qnx.qnx800.quickstart.qemu`, installed with the Software Center into the same SDP (the headless client takes `-installPackage com.qnx.qnx800.quickstart.qemu -destination /qnx/qnx800`). It is an x86_64 image only, so on Apple Silicon there is no hardware acceleration:

```bash
mkdir -p ~/qnx/work/quickstart && cd ~/qnx/work/quickstart
cat ~/qnx/qnx800/images/qemu/*.tar.gz.* | tar xzf -
qemu-img convert -O qcow2 qemu/output/disk-qemu qemu/output/disk.qcow2
rm qemu/output/disk-qemu            # the raw disk is sparse but expands to 47 GB; the qcow2 is about 5 GB
cd -
```

Create an ssh key and boot the VM (4 vCPU, 4 GB, ssh on `localhost:2223`, serial console on TCP 4555):

```bash
ssh-keygen -t ed25519 -N '' -f ~/qnx/vm_key
(scripts/run-vm-quickstart.sh > /dev/null 2>&1 &); sleep 90
```

The quickstart image refuses root ssh login but accepts `qnxuser`. Install your public key once through the serial console (`nc 127.0.0.1 4555`, login `root` with password `root`, the image defaults) by appending `~/qnx/vm_key.pub` to `/data/home/qnxuser/.ssh/authorized_keys`, with the directory owned by `qnxuser` and mode 700 (file mode 600). Then check:

```bash
VM=quickstart scripts/vm-ssh.sh 'echo connected to QNX'
```

### Target B: AWS Graviton with QNX OS 8.0

1. In the AWS Marketplace, subscribe to the **QNX OS 8.0** AMI for arm64 (QNX documents connecting in the `qnxcloud.ami` docs, "Connecting to an Existing QNX AMI Instance").
2. Launch it on a `c7g.xlarge`, with your own key pair, and a security group that allows **SSH (port 22) only from your own IP address**. The login user is `qnxuser`.
3. Tell the scripts where it is, then check the connection:

```bash
mkdir -p ~/qnx/aws
echo "<instance public IP>" > ~/qnx/aws/ip
cp /path/to/your-key.pem ~/qnx/aws/qnx-hackathon.pem && chmod 600 ~/qnx/aws/qnx-hackathon.pem
scripts/aws-ssh.sh 'echo connected to QNX'
```

The public IP changes every time you stop and start the instance, so refresh `~/qnx/aws/ip` after a restart. The QNX shell on the AMI is minimal (no `head`, `tail` or `nproc`), so keep remote commands simple. **Stop the instance when you finish** (do not leave it running):

```bash
aws ec2 stop-instances --region <region> --instance-ids <your-instance-id>
```

## Stage the files
Duration: 5:00

Staging copies `voicecmd`, the shared libraries and the model to the target. The test streams (20 spoken commands, made with macOS `say`) are optional; to make them, see the last section.

```bash
scripts/stage-vm.sh            # Target A: QEMU, files go to /data/home/qnxuser/x
scripts/stage-aws.sh           # Target B: Graviton, files go to ~/x
```

## Run the file test
Duration: 10:00

First run the grammar unit test on your host. It is plain C++17 with no whisper dependency, so it needs only a compiler (78 cases, all should pass):

```bash
c++ -std=c++17 -Ivoicecmd/src voicecmd/src/grammar.cpp voicecmd/tests/test_grammar.cpp -o /tmp/test_grammar && /tmp/test_grammar | tail -2
```

The same test is also built for QNX as `~/qnx/work/vc-<arch>/test_grammar` and passed on the QNX instance.

Then run the recogniser on a 20-phrase test stream on the target (make `streamA.wav` first, see "Make your own test streams"):

```bash
# Target A (QEMU): files were staged to /data/home/qnxuser/x
VM=quickstart scripts/vm-ssh.sh 'cd /data/home/qnxuser/x/gen && LD_LIBRARY_PATH=$PWD/lib ./voicecmd -m ../vc/ggml-tiny.en.bin -a file:../vc/streamA.wav -t 4 2>/dev/null'
# Target B (Graviton): files were staged to ~/x
scripts/aws-ssh.sh 'cd x/gen && LD_LIBRARY_PATH=$PWD/lib ./voicecmd -m ../vc/ggml-tiny.en.bin -a file:../vc/streamA.wav -t 4 2>/dev/null'
```

(`stage-vm.sh` and `stage-aws.sh` copy `stream[ABCD].wav` from `~/qnx/work/demo` if they exist, so make the streams before staging, or re-run staging afterwards.)

Optional plain whisper test: `scripts/transcribe-on-target.sh` pushes `whisper-cli`, the model and `jfk.wav` to a QNX 8.0 x86_64 image built with the SDP's `mkqnximage` (`scripts/make-image.sh`, which needs `~/qnx/vm_key.pub`; boot it with `scripts/run-vm-x86_64.sh`, ssh as `root` on `localhost:2222`) and prints the transcript and timings. Expected transcript: "And so my fellow Americans ask not what your country can do for you / ask what you can do for your country." Measured for this 11 s clip: 77 s on QEMU (real-time factor about 7), 1.76 s on Graviton.

Each `voicecmd` result is one JSON line, for example (the 20-phrase stream on Graviton):

```
{"event":"ready","audio":"file","model_load_ms":470,"threads":4}
{"event":"command","intent":"set_temperature","slots":{"celsius":21},"match":"exact","transcript":"Set temperature to 21.","audio_ms":...,"latency_ms":...}
...
{"event":"rejected","reason":"out_of_range",...}
```

Score a run against the expected answers with the project's scorer:

```bash
scripts/bench-aws.sh streamA.wav aws -t 4       # on Graviton: prints accuracy and latency
```

The measured result of the 20-phrase set on Graviton was 20/20 at about 130 to 145 ms per command (QEMU: 20/20 at about 7.9 s).

## Live microphone and dashboard
Duration: 10:00

The host captures the microphone with `ffmpeg`, converts it to 16 kHz mono signed 16-bit PCM, and listens on a local TCP port. `voicecmd` on QNX connects to it with `-a tcp:...`: on QEMU the host is visible to the guest as `10.0.2.2`; on AWS an `ssh -R` reverse tunnel makes the host port appear as `127.0.0.1` on the instance. Events come back over the ssh session and are piped into the dashboard.

List your audio inputs (optional), then start the demo:

```bash
scripts/mac-mic-stream.sh --list           # macOS: pick an index; default input is used unless MIC=<index>

# Target A: QEMU
scripts/demo-live.sh

# Target B: Graviton (pick any free local port)
PORT=5575 scripts/demo-live-aws.sh
```

Add `--wav file.wav` to either script to play a recorded file as if it were the microphone. The dashboard opens at http://127.0.0.1:8090. Wait for "listening", then speak one command at a time, with a short pause between them:

* "Set temperature to twenty two"
* "Navigate home"
* "Call Sarah"
* "Volume up", "Mute"
* "Open the passenger window"
* "Can you play Spotify", "Next track", "Pause"

Things that should be rejected with a reason, and change nothing: "Set temperature to ninety nine", "Call Zebediah", "Navigate to Paris", "Open the door", "What is the weather like today".

The event log is also written to `/tmp/voicecmd-demo-events.jsonl` (override with `TEE=...`). Ctrl-C stops the demo. Measured on Graviton with a live microphone on 2026-11-26: 7 of 7 car commands and 5 of 5 media commands correct at 108 to 142 ms, and every non-command rejected.

## How the pipeline works
Duration: 10:00

`voicecmd/src/voicecmd.cpp` ties four small pieces together:

1. **Audio source** (`audio_source.c`): one interface, 16 kHz mono s16, with three backends: `file:x.wav`, `tcp:host:port` and `snd:<pcm>` (QNX io-snd, ALSA-style API from `alsa/asoundlib.h`, built with `SND=1 scripts/build-voicecmd.sh x86_64`, untested at runtime).
2. **VAD** (`vad.h`): 20 ms frames, a noise floor calibrated on the first 300 ms and slowly adapted while a gate is open, a 10 dB margin, 600 ms hangover and 200 ms preroll. Only the speech segment goes to whisper, which keeps the model idle in silence.
3. **whisper.cpp**: `tiny.en`, greedy decoding, an initial prompt containing example commands (this biases the decoder toward the vocabulary), `audio_ctx` shrunk to the utterance length plus a margin, and at most 10 tokens. Shrinking the context is what turns a 1 to 3 s command into about 110 to 160 ms on Graviton. A side effect is that whisper repeats short phrases, which `collapse_repeats` removes.
4. **Grammar** (`grammar.cpp`): lower-case, strip punctuation and filler words ("please", "can you"), read number words ("twenty four"), repair a near-miss anchor word (edit distance 1, unique match), then map to an intent with checked slots (temperature 16 to 30, a known contact, a known destination, a known media source). Anything else is a typed rejection. This is deliberately a layer after whisper and not whisper's own grammar mode: tiny.en token boundaries make a token grammar fragile.

Tunables on the command line: `--prompt TEXT`, `--no-prompt`, `--ctx-margin N`, `--audio-ctx-min N`, `--max-tokens N`, `--hang-ms N`, `--timings`, `-t threads`.

## Add your own command
Duration: 10:00

1. Add test lines to `voicecmd/tests/test_grammar.cpp` for the phrase and for a near miss you want rejected.
2. Add the intent in `voicecmd/src/grammar.cpp` (keywords, slot check, `accept("my_intent", "{...}", norm, fuzzy)`), and extend `voicecmd/src/grammar.h` comments.
3. If the phrase should be recognised more reliably, add it to `command_prompt()`. Re-run the benchmark when you change the prompt: a longer prompt costs a little time (about 10 to 15 ms on Graviton for the media phrases) and can bias the decoder, for example towards a known contact name.
4. Add the intent to the state handling in `demo/dashboard.py`.
5. Re-run the host grammar test (previous section), then rebuild, restage and run:

```bash
scripts/build-voicecmd.sh aarch64le && scripts/stage-aws.sh        # or x86_64 and scripts/stage-vm.sh
```

## Make your own test streams
Duration: 10:00

On macOS, record 20 phrases with `say`, join them with silence, and add cabin-like noise. The expected result for each phrase is in `voicecmd/data/commands.tsv`:

```bash
mkdir -p ~/qnx/work/demo && cd ~/qnx/work/demo
say -v Samantha -o c01.wav --data-format=LEI16@16000 "Set temperature to 21"
# ... one file per line of voicecmd/data/commands.tsv, named c01.wav to c20.wav
python3 -I ~/path/to/whisper-qnx/voicecmd/data/make_stream.py streamA.wav c*.wav
python3 -I ~/path/to/whisper-qnx/voicecmd/data/add_noise.py streamA.wav streamB.wav 10    # 10 dB cabin noise
```

`stage-aws.sh` and `bench-aws.sh` look for `stream[ABCD].wav` in `~/qnx/work/demo`.

## Troubleshooting
Duration: 5:00

* **`qcc` says the licence is not activated.** The container must be started with the fixed `--hostname qnx-dev --mac-address 02:42:ac:11:00:42`, as root with `HOME=/root` and `~/qnx/dotroot` mounted at `/root` (use `scripts/qnx-container.sh`).
* **The Docker image build fails on Python.** The upstream QNX Dockerfile pulled `python3.11` from a PPA that no longer serves Ubuntu 20.04. This project's `docker/qnx-build/Dockerfile` avoids it.
* **CMake is too old.** whisper.cpp needs more than Ubuntu 20.04's CMake 3.16; the image installs 3.22.
* **Build error about `_SC_PHYS_PAGES`.** The patch in `patches/` was not applied; re-run `scripts/build.sh`.
* **First live run has no audio.** macOS needs microphone permission for the Terminal app you use. Check with `scripts/mac-mic-stream.sh --list`.
* **Port already in use.** Set `PORT=<free port>`. On the author's machine 5555 was held by an Android emulator.
* **Cannot reach the AWS instance after a restart.** The public IP changed: update `~/qnx/aws/ip`. If your own IP changed, authorize the new one in the security group.
* **"Can you play Spotify" shows as rejected.** `tiny.en` can hear "play" as "please". The grammar accepts a lone source word as `play_media`; if you changed the prompt, re-run the benchmark.
* **QEMU is slow.** Expected: it is emulation (about 8 s per command). An AVX2 build is slower under emulation, so leave `SIMD` off.

## Summary and limitations
Duration: 3:00

You cross-built whisper.cpp and a voice-command daemon for QNX 8.0, ran them on a QNX target, and drove a dashboard with spoken commands.

Measured results (TTS voices, 20 phrases per set, `tiny.en`, 4 threads):

| Set | QEMU x86_64 | AWS Graviton3, QNX OS 8.0 |
|---|---|---|
| Voice A clean | 7.9 s, 20/20 | 128 ms, 20/20 |
| Voice B 10 dB noise | not run | 119 ms, 19/20 |
| Voice C clean | 7.9 s, 20/20 | 125 ms, 20/20 |
| Voice D 10 dB noise | 7.8 s, 18/20 | 111 ms, 18/20 |

Limitations to keep in mind:

* The benchmarks use synthetic (text-to-speech) voices and synthetic noise; they are a smoke test, not a benchmark of real drivers.
* Under noise, "Call Zebediah" was heard as "Call Sarah", a wrong action caused by the prompt bias. A real system would confirm calls.
* Audio reaches QNX over the network; the native io-snd backend has not been run.
* The AWS Graviton3 instance is real QNX OS 8.0, but it is a cloud server, not an automotive board.
* English only, fixed vocabulary, energy-based VAD.

Source, scripts, results and raw logs: https://github.com/satsxrands/whisper-qnx
