#!/bin/bash
# run_pad.sh <etiquette> [secondes] : lance l'appli installee avec le parcours pad8.txt (dev_env.txt modifie puis restaure)
export MSYS_NO_PATHCONV=1
M="$(dirname "$0")"; A="/c/Android/platform-tools/adb "; TAG=${1:?}; SEC=${2:-150}
F=/sdcard/Android/data/org.nightjar.natif/files
$A push "$M/pad8.txt" $F/pad8.txt >/dev/null
$A shell "cp $F/dev_env.txt $F/dev_env.txt.n3sav && echo RE4_PADFILE=$F/pad8.txt >> $F/dev_env.txt; for l in ${EXTRA:-}; do echo \$l >> $F/dev_env.txt; done"
$A shell "am force-stop org.nightjar.natif; input keyevent KEYCODE_WAKEUP; am start -n org.nightjar.natif/org.nightjar.re4.RE4Activity >/dev/null; sleep $SEC"
$A exec-out screencap -p > "$M/${TAG}_screen.png"
python -c "from PIL import Image; im=Image.open(r'$M/${TAG}_screen.png'); im.thumbnail((480,270)); im.save(r'$M/${TAG}_small.png')"
$A shell "mv $F/dev_env.txt.n3sav $F/dev_env.txt; sha256sum $F/dev_env.txt | cut -c1-16"
$A shell "echo coeur_vivant=\$(ps -A | grep -c njcore); grep -a -E 'NJGFX\] image|signal|OSPanic|PAD :' $F/core.log | tail -8" | cut -c1-200
MSYS_NO_PATHCONV=1 $A pull $F/core.log "$M/${TAG}_core.log" >/dev/null 2>&1
