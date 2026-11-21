#!/bin/bash
exec ssh -i $HOME/qnx/vm_key -p 2222 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o IdentitiesOnly=yes -o ConnectTimeout=20 root@127.0.0.1 "$@"
