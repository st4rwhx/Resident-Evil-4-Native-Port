#!/bin/bash
# variant.sh <unit> <variant-src> [FUNC...] [--out DIR] [--cc1dir DIR] [--all] [--no-diff]
# Compile a variant source of one unit with its exact production command (object under /tmp,
# build/ untouched), run bytecmp on it and print a side-by-side disassembly diff. See README.md.
exec python3 "$(dirname "$(readlink -f "$0")")/variant.py" "$@"
