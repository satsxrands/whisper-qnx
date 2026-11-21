#!/bin/bash
# ssh into the VM. Default: mkqnximage VM (root, port 2222). Quickstart VM: VM=quickstart (qnxuser, port 2223; root login is refused there).
if [ "$VM" = quickstart ]; then P=2223; U=qnxuser; else P=2222; U=root; fi
exec ssh -i $HOME/qnx/vm_key -p $P -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o IdentitiesOnly=yes -o ConnectTimeout=20 $U@127.0.0.1 "$@"
