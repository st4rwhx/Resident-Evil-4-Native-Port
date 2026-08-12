#!/bin/bash
# Phase N1 : notre port natif (re4-decomp-upstream + port/) compile sur le PORTABLE (ARM32, -O2 par defaut).
#   laptop_port_build.sh [PORT_OPT]
set -e
H=admin@192.168.1.124
S="C:/Users/Admin/Downloads/RE4 PROJECT/re4-decomp-upstream"
OPT="${1:--O2}"
ssh -o ConnectTimeout=8 $H "mkdir -p /c/re4n/re4port"
tar cf - -C "$S" --exclude=./.git --exclude=./_port_build --exclude=./_port_dbg . | ssh $H "cd /c/re4n/re4port && tar xf - 2>/dev/null"
# collision de casse src/Tools / src/tools (NTFS) : contenus pris dans git, minuscules -> src/tools_lc, modules.py ajuste
python "$(dirname "$0")/casse_tar.py" "$S" | ssh $H "cd /c/re4n/re4port && tar xf - && sed -i 's|\"tools/|\"tools_lc/|g' config/G4BE08/modules.py && grep -rl \"#include \\\"tools/\" src include | xargs -r sed -i 's|#include \"tools/|#include \"tools_lc/|g'"
ssh $H "cd /c/re4n/re4port && find port src include -mmin -600 -type f -exec touch {} + 2>/dev/null; true"
# ordre d'origine : objets du DOL (build_rel lit _port_build/obj pour savoir ce que le DOL definit), modules REL, puis lien
ssh -o ServerAliveInterval=15 $H "cd /c/re4n/re4port && T0=\$(date +%s) && export NDK=C:/android/android-ndk-r26d PORT_OPT='$OPT' && /c/Python314/python port/build_game.py --jobs 12 --quiet > /c/re4n/re4port_dol.log 2>&1; tail -1 /c/re4n/re4port_dol.log; /c/Python314/python port/build_rel.py --jobs 12 > /c/re4n/re4port_rel.log 2>&1; tail -3 /c/re4n/re4port_rel.log; /c/Python314/python port/build_game.py --link --quiet --jobs 12 > /c/re4n/re4port_build.log 2>&1; R=\$?; echo \"build=\$R duree=\$(( \$(date +%s) - T0 )) s\"; tail -12 /c/re4n/re4port_build.log | cut -c1-220; ls -la _port_build/re4 2>&1"
