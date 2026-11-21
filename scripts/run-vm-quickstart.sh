#!/bin/bash
# Boot the QNX 8.0 QEMU quickstart image (x86_64 ONLY: the 20260902 quickstart ships no aarch64 image, so no hvf on Apple Silicon).
# Unpack once (see README): ~/qnx/work/quickstart/qemu/output/{ifs.bin,disk.qcow2}. ssh on localhost:2223, serial console on tcp 4555.
# AUDIO=usb adds qemu-xhci + usb-audio (QNX devu-hcd-xhci + sndrv-ctrl-usb); AUDIO=hda adds intel-hda + hda-duplex.
cd "$HOME/qnx/work/quickstart/qemu/output"
EXTRA=""
case "$AUDIO" in
  usb) EXTRA="-audiodev none,id=snd0 ${USBCTL:--device qemu-xhci,id=usb} -device usb-audio,audiodev=snd0,bus=usb.0" ;;
  hda) EXTRA="-audiodev none,id=snd0 -device intel-hda,msi=on -device hda-duplex,audiodev=snd0" ;;
esac
exec qemu-system-x86_64 $EXTRA -nographic -display none -m ${MEM:-4G} -smp ${SMP:-4} -cpu max -accel tcg,thread=multi \
  -drive file=disk.qcow2,if=ide,id=drv0 -kernel ifs.bin \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2223-:22 -device virtio-net-pci,netdev=net0 \
  -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-pci,rng=rng0 \
  -serial tcp:127.0.0.1:4555,server,nowait -monitor none -pidfile qemu.pid
