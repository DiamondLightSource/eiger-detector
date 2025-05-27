#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd $SCRIPT_DIR

grep -qE '127\.0\.0\.1.*\bodin-data-1\b' /etc/hosts || sed '/^127\.0\.0\.1/ s/$/ odin-data-1 odin-data-2 odin-data-3 odin-data-4 eiger-fan meta-writer control-server/' /etc/hosts >/tmp/hosts && cat /tmp/hosts >/etc/hosts && rm /tmp/hosts

zellij -l ./layout.kdl
