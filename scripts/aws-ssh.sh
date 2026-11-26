#!/bin/bash
# ssh to the QNX OS 8.0 AWS Graviton instance (c7g.xlarge, AMI QNX OS 8.0.5 arm64). Login user qnxuser (QNX AMI default).
# Reads the public IP from $AWS_QNX_IP or ~/qnx/aws/ip; key ~/qnx/aws/qnx-hackathon.pem (outside git).
# usage: aws-ssh.sh [-R spec | -L spec]... [command]
IP="${AWS_QNX_IP:-$(cat ~/qnx/aws/ip)}"; OPTS=()
while [ "${1#-}" != "$1" ] && [ -n "$2" ]; do OPTS+=("$1" "$2"); shift 2; done
exec ssh -i "${AWS_QNX_KEY:-$HOME/qnx/aws/qnx-hackathon.pem}" -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -o ServerAliveInterval=20 "${OPTS[@]}" qnxuser@"$IP" "$@"
