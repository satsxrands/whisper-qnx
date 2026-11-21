#!/bin/bash
# Build the QNX 8.0 x86_64 QEMU image with the SDP's mkqnximage (inside the licence-pinned container).
# Needs ~/qnx/vm_key.pub (throwaway ssh-keygen -t ed25519 -f ~/qnx/vm_key). Output: ~/qnx/vm-x86_64.
# aarch64le is BLOCKED: mkqnximage needs startup-qemu-virt from the QEMU quickstart BSP package, which
# needs the QNX Software Center to install (com.qnx.qnx800.quickstart.qemu); the base SDP lacks it.
set -e
mkdir -p "$HOME/qnx/vm-x86_64"
"$(dirname "$0")/qnx-container.sh" 'cd /qnx/vm-x86_64 && mkqnximage --type=qemu --arch=x86_64 --ssh-ident=/qnx/vm_key.pub --ram=4G --cpu=4 --data-size=1024 --data-inodes=2000 --noprompt --build'
