#!/bin/bash
# Premiere compilation iOS du natif (18/11) sur le PORTABLE : synchro (meme que laptop_port_build.sh) + build_game.py PORT_ABI=ios
set -e
H=user@laptop
S="."
ssh -o ConnectTimeout=8 $H "mkdir -p /c/re4n/re4port"
tar cf - -C "$S" --exclude=./.git --exclude=./_port_build --exclude=./_port_dbg . | ssh $H "cd /c/re4n/re4port && tar xf - 2>/dev/null"
python "$(dirname "$0")/casse_tar.py" "$S" | ssh $H "cd /c/re4n/re4port && tar xf - && sed -i 's|\"tools/|\"tools_lc/|g' config/G4BE08/modules.py && grep -rl \"#include \\\"tools/\" src include | xargs -r sed -i 's|#include \"tools/|#include \"tools_lc/|g'"
ssh $H "cd /c/re4n/re4port && find port src include -mmin -600 -type f -exec touch {} + 2>/dev/null; true"
ssh -o ServerAliveInterval=15 $H 'cd /c/re4n/re4port && T0=$(date +%s) && export NDK=C:/android/android-ndk-r26d PORT_ABI=ios && /c/Python314/python port/build_game.py --jobs 12 --quiet --errlog /c/re4n/ios_natif_err.txt > /c/re4n/ios_natif_dol.log 2>&1; tail -2 /c/re4n/ios_natif_dol.log; echo "duree $(( $(date +%s) - T0 )) s"; grep -c "error:" /c/re4n/ios_natif_err.txt; /c/Python314/python port/build_rel.py --jobs 12 --errlog /c/re4n/ios_natif_rel_err.txt > /c/re4n/ios_natif_rel.log 2>&1; tail -3 /c/re4n/ios_natif_rel.log; grep -c "error:" /c/re4n/ios_natif_rel_err.txt; grep -h "error:" /c/re4n/ios_natif_rel_err.txt | sed "s/.*error: //" | sort | uniq -c | sort -rn | head -8'
