#!/bin/bash
# rtl.sh <unit> <variant-src> [--out DIR] [--cc1dir DIR] [-dX ...]
# GCC units only: compile the variant with the production command plus the RTL dump flags
# (-dj -ds -dS -dR -dg -dl -dG -dL -fsched-verbose-9, or the -dX flags you pass instead) and keep
# the .i and every dump in the out directory. See README.md.
exec python3 "$(dirname "$(readlink -f "$0")")/variant.py" --rtl "$@"
