#!/usr/bin/env bash
# A/B entrelace sur le telephone : chaque bras tour a tour, meme salle, memes conditions (essai.sh
# attend temperature et batterie), puis moyenne [FRAME] des images en jeu (tools/re4/frames.py).
#
#   android/ab.sh <bras> <bras> [tours] [NJ_X=valeur ...]
#   bras : nom=chemin.apk[,NJ_Y=val[,NJ_Z=val]]  (interrupteurs propres a ce bras)
# Les NJ_X=valeur en fin de ligne valent pour les deux bras. Resultat : $OUT (C:/re4n/apk/ab.txt).
# Frequence fixe : android/freq.sh fixe avant, freq.sh libre apres.
set -u
export MSYS_NO_PATHCONV=1
HERE="$(cd "$(dirname "$0")" && pwd)"
ADB=${ADB:-/c/android/sdk/platform-tools/adb.exe}
OUT=${OUT:-C:/re4n/apk/ab.txt}
A=$1; B=$2; TOURS=${3:-2}; shift 3 || shift $#
for t in $(seq 1 "$TOURS"); do
    for pair in "$A" "$B"; do
        nom=${pair%%=*}; rest=${pair#*=}
        apk=${rest%%,*}
        extra=()
        [ "$rest" != "$apk" ] && IFS=',' read -r -a extra <<<"${rest#*,}"
        res=$(APK="$apk" ATTENTE=${ATTENTE:-25} RPROF=0 bash "$HERE/essai.sh" -i NJ_FRAMELOG=0,100000 "${extra[@]}" "$@" 2>&1 | grep -E "depart|pas en jeu|refusee" | head -1)
        fr=$(python "$(cygpath -m "$HERE/../tools/re4/frames.py")" 120 2>&1 | tail -3 | tr '\n' ' ')
        echo "tour $t $nom | $res | $fr" | tee -a "$OUT"
    done
done
