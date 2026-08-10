#!/bin/sh
# Construit, envoie et execute tout le portage sur le telephone, en une commande.
#
#   NDK=<racine android-ndk-r26d> ADB=<adb.exe> ISO=<image du jeu> sh port/run_on_device.sh
#
# Sans ISO, seuls les harnais de verification tournent - ils n'ont besoin d'aucune donnee du
# jeu. Avec ISO, le jeu lui-meme demarre a la suite.
set -e
export MSYS_NO_PATHCONV=1

: "${NDK:?definir NDK sur la racine de android-ndk-r26d}"
: "${ADB:=adb}"
ROOT="$(cygpath -m "$(cd "$(dirname "$0")/.." && pwd)" 2>/dev/null || (cd "$(dirname "$0")/.." && pwd))"
OUT="${OUT:-$ROOT/_port_build}"
DEV=/data/local/tmp/re4

echo "== harnais de verification =="
NDK="$NDK" ADB="$ADB" OUT="$OUT" sh "$ROOT/port/build_tests.sh"
for t in test_texture test_gx test_osdvd test_egl; do
    echo
    echo "---- $t"
    "$ADB" shell "$DEV/$t" || true
done

echo
echo "== le jeu =="
NDK="$NDK" python "$ROOT/port/build_game.py" --out "$OUT" --link --quiet
"$ADB" push "$OUT/re4" "$DEV/re4" >/dev/null
"$ADB" shell chmod 755 "$DEV/re4"

if [ -n "$ISO" ]; then
    # L'image n'est envoyee que si elle n'est pas deja la : 1,4 Go prennent une minute.
    if ! "$ADB" shell "[ -f $DEV/re4.iso ]"; then
        echo "envoi de l image du jeu (une seule fois)..."
        "$ADB" push "$ISO" "$DEV/re4.iso"
    fi
    "$ADB" shell "cd $DEV && ./re4 $DEV/re4.iso"
else
    echo "(pas d ISO fourni : le jeu n'est pas lance)"
fi
