#!/usr/bin/env sh
D=$(cd "$(dirname "$0")" && pwd)
export LD_LIBRARY_PATH="$D/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export TERM=xterm
export TERMINFO="$D/share/terminfo"
exec "$D/bin-util/joypi_controller" "$@"
