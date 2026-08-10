#!/bin/sh
# dis26.sh START END  (hex VAs) -> objdump intel disassembly of GC/2.6 mwcceppc.exe (dis26.sh 507c70 507d80 = the scheduler entry) without byte columns
RE4_ROOT=${RE4_ROOT:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}
objdump -d -M intel --start-address=0x$1 --stop-address=0x$2 "$RE4_ROOT/build/compilers/GC/2.6/mwcceppc.exe" | awk -F'\t' 'NF>=3{sub(/^ +/,"",$1); print $1" "$3}'
