#!/bin/bash
# Boot the QNX 8.0 x86_64 image headless under QEMU TCG; ssh on localhost:2222, serial log in ~/qnx/vm-x86_64/serial.log
cd "$HOME/qnx/vm-x86_64"
exec qemu-system-x86_64 -nographic -display none -m 4G -smp 4 -cpu max -accel tcg,thread=multi \
  -drive file=output/disk-qemu.vmdk,if=ide,id=drv0 -kernel output/ifs.bin \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 -device virtio-net-pci,netdev=net0 \
  -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-pci,rng=rng0 \
  -serial file:serial.log -monitor none -pidfile qemu.pid
