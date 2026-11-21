#!/bin/bash
# run a command inside the licence-pinned QNX container; cwd /qnx/vm-$ARCH
export PATH=$HOME/.rd/bin:$PATH
exec docker run --rm -u root -e HOME=/root --platform linux/amd64 --hostname qnx-dev --mac-address 02:42:ac:11:00:42 -v $HOME/qnx:/qnx -v $HOME/qnx/dotroot:/root qnx800:latest bash -c "source /qnx/qnx800/qnxsdp-env.sh >/dev/null; $*"
