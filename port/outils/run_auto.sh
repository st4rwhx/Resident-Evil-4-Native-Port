#!/bin/bash
# run_auto.sh <etiquette> <debut_s> <fin_s> [pas_s=10] : banc d'essai fiable (phase A, PLAN_NATIF_REPRISE_2026-11-12.md).
#  - dev_env.txt verifie AVANT (empreinte 8282fab8, sinon arret), ajout temporaire de RE4_AUTOLOAD=1 (+ EXTRA="VAR=val ..."),
#    restauration APRES, meme sur interruption (trap) ;
#  - pilote par l'etat du jeu (port/pad.cpp RE4_AUTOLOAD : NEW GAME -> r100, puis plus aucune touche) ;
#  - captures toutes les <pas> s entre <debut> et <fin>, planche, journal du coeur, mesures [NJGFX].
export MSYS_NO_PATHCONV=1
M="$(dirname "$0")"; A="/c/Android/platform-tools/adb "; TAG=${1:?}; T1=${2:-90}; T2=${3:-200}; STEP=${4:-10}
F=/sdcard/Android/data/org.nightjar.natif/files
WANT=8282fab82924588e
# batterie : jamais d'essai sous 40 % (le S5 s'eteint sinon) ; MIN_BAT=<n> pour changer le seuil
LVL=$($A shell "dumpsys battery | grep level" | tr -dc '0-9')
if [ -z "$LVL" ] || [ "$LVL" -lt "${MIN_BAT:-40}" ]; then echo "ARRET : batterie a ${LVL:-?} % (minimum ${MIN_BAT:-40})"; exit 1; fi
H=$($A shell "sha256sum $F/dev_env.txt" | cut -c1-16)
if [ "$H" != "$WANT" ]; then echo "ARRET : dev_env.txt modifie ($H)"; exit 1; fi
restore() { $A shell "am force-stop org.nightjar.natif; if [ -f $F/dev_env.txt.n3sav ]; then mv $F/dev_env.txt.n3sav $F/dev_env.txt; fi"; }
trap restore EXIT INT TERM
$A shell "cp $F/dev_env.txt $F/dev_env.txt.n3sav && echo RE4_AUTOLOAD=${ALOAD:-1} >> $F/dev_env.txt; for l in ${EXTRA:-}; do echo \$l >> $F/dev_env.txt; done"
# EXTRA_FILE=<fichier local> : lignes ajoutees telles quelles (valeurs avec ; comme NJ_NATIVE_SETTINGS), la derniere l'emporte
if [ -n "${EXTRA_FILE:-}" ]; then $A push "$EXTRA_FILE" $F/extra_env.txt >/dev/null && $A shell "cat $F/extra_env.txt >> $F/dev_env.txt; rm $F/extra_env.txt"; fi
$A shell "rm -f $F/core.log"   # pas de journal d'un essai precedent pris pour celui-ci
$A shell "am force-stop org.nightjar.natif; input keyevent KEYCODE_WAKEUP; am start -n org.nightjar.natif/org.nightjar.re4.RE4Activity >/dev/null"
rm -rf "$M/shots_$TAG"; mkdir -p "$M/shots_$TAG"
T0=$(date +%s)
sleep "$T1"
i=0
while [ $(( $(date +%s) - T0 )) -lt "$T2" ]; do
  $A exec-out screencap -p > "$M/shots_$TAG/s$(printf %02d $i)_$(( $(date +%s) - T0 )).png"
  i=$((i + 1))
  sleep "$STEP"
done
restore
trap - EXIT INT TERM
H2=$($A shell "sha256sum $F/dev_env.txt" | cut -c1-16)
[ "$H2" = "$WANT" ] && echo "dev_env ok" || echo "ATTENTION dev_env $H2"
$A pull $F/core.log "$M/${TAG}_core.log" >/dev/null 2>&1
python - "$M/shots_$TAG" "$M/${TAG}_planche.png" <<'PY'
import sys, os
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
fs = sorted(f for f in os.listdir(d) if f.endswith('.png'))
ims = []
for f in fs:
    im = Image.open(os.path.join(d, f)).convert('RGB')
    im.thumbnail((320, 180))
    ImageDraw.Draw(im).text((4, 4), f.split('_')[1][:-4] + 's', fill=(255, 255, 0))
    ims.append(im)
cols = 3
rows = max(1, (len(ims) + cols - 1) // cols)
sheet = Image.new('RGB', (320 * cols, 180 * rows))
for k, im in enumerate(ims):
    sheet.paste(im, ((k % cols) * 320, (k // cols) * 180))
sheet.save(out)
print(out, len(ims), 'captures')
PY
grep -a "\[NJGFX\] mesure" "$M/${TAG}_core.log" | awk 'NR%2==0' | tail -10 | cut -c1-130
grep -a "signal \|OSPanic" "$M/${TAG}_core.log" | head -3
