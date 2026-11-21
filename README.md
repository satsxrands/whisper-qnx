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
