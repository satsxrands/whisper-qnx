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

## Run
`scripts/transcribe-on-target.sh` copies whisper-cli, libs, ggml-tiny.en and jfk.wav to /data/w and runs
`LD_LIBRARY_PATH=lib ./bin/whisper-cli -m ggml-tiny.en.bin -f jfk.wav -t 4`.

## Results
jfk.wav (11.0 s), tiny.en, 4 threads, QEMU TCG on Apple Silicon (software emulation, generic CPU kernels):
"And so my fellow Americans ask not what your country can do for you / ask what you can do for your country."
Total 88.9 s (model load 6.3 s), real-time factor about 8.1 (emulated; not representative of real hardware).
