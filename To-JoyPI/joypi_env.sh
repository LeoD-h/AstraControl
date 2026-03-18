#!/usr/bin/env sh
# Source: . ./joypi_env.sh  (ou utiliser run_*.sh directement)
D=$(cd "$(dirname "$0")" && pwd)
export LD_LIBRARY_PATH="$D/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export TERM=xterm
export TERMINFO="$D/share/terminfo"
echo "env OK: TERMINFO=$TERMINFO  TERM=$TERM"
