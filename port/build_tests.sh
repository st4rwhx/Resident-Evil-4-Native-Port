#!/bin/sh
# Construit les harnais du portage pour l'appareil, les pousse, et les execute.
#
# Ecrit en script plutot que retape a chaque fois parce que trois details ont deja coute du
# temps et se represenent a chaque session :
#   * sous Git Bash, MSYS_NO_PATHCONV=1 est obligatoire, sinon /data/local/tmp devient un
#     chemin Windows et adb ne trouve rien ;
#   * adb push **remet les permissions a zero** : il faut rechmod 755 apres chaque envoi ;
#   * les chemins contenant des espaces : tout -I doit etre entre guillemets.
#
# usage : NDK=<racine du ndk> ADB=<adb.exe> sh port/build_tests.sh [cible ...]
set -e
export MSYS_NO_PATHCONV=1

: "${NDK:?definir NDK sur la racine de android-ndk-r26d}"
: "${ADB:=adb}"
CXX="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/armv7a-linux-androideabi28-clang++"
# MSYS_NO_PATHCONV=1 protege les chemins **de l appareil**, mais il empeche aussi bash de
# convertir /c/... pour un clang qui est un binaire Windows : on convertit donc a la main,
# en barres obliques (cygpath -m), la forme que clang accepte y compris en fichier reponse.
ROOT="$(cygpath -m "$(cd "$(dirname "$0")/.." && pwd)")"
OUT="${OUT:-$ROOT/_port_build}"
case "$OUT" in /*) OUT="$(cygpath -m "$OUT")" ;; esac
DEV=/data/local/tmp/re4

mkdir -p "$OUT"

# -std=gnu++11 : c'est gnu++11 qui accepte les unions non restreintes du decompile, et surtout
# **pas** gnu++17, qui supprime `register` et rouvre 44 erreurs. -fdeclspec en resout 369.
# -DDEBUG=1 n est pas un choix de confort, c est ce qui rend le portage possible :
#   * GXVert.h ne genere les fonctions de soumission de sommets en **externes** que sous
#     DEBUG - sinon elles sont inline et ecrivent dans le write-gather pipe du materiel ;
#   * os.h masque OSPhysicalToCached et consorts par des macros sous #if !DEBUG, macros qui
#     rendent les souches correspondantes inecrivables.
CFLAGS="-std=gnu++11 -fdeclspec -DRE4_PORT -DDEBUG=1 -Oz -fno-exceptions -fno-rtti"
# Deux -I **separes** et chacun entre guillemets pour proteger les chemins contenant des espaces.
INC1="-I$ROOT/include"
INC2="-I$ROOT/port"

# Les sources de plateforme dont chaque harnais a besoin. texobj.cpp et gx.cpp tirent GLES.
# Les listes de sources passent par un **fichier reponse** pour gerer proprement les espaces.
# Un fichier reponse met un chemin par ligne, sans ambiguite.
PLAT_CORE="os dvd vi pad port_log stubs_generated"
PLAT_GL="gx texture texobj"

build() {
    name="$1"; shift
    echo "-- $name"
    rsp="$OUT/$name.rsp"
    : > "$rsp"
    for u in "$@"; do
        # Chaque ligne entre guillemets : meme dans un fichier reponse, clang coupe
        # un chemin sur son espace.
        printf '"%s"
' "$ROOT/port/$u.cpp" >> "$rsp"
    done
    "$CXX" $CFLAGS "$INC1" "$INC2" -o "$OUT/$name" "@$rsp" -lEGL -lGLESv3 -llog -static-libstdc++
}

TARGETS="${*:-test_texture test_gx test_osdvd test_egl}"

for t in $TARGETS; do
    case "$t" in
    test_texture) build "$t" test_texture texture port_log ;;
    test_egl)     build "$t" test_egl ;;
    test_gx)      build "$t" test_gx $PLAT_GL $PLAT_CORE ;;
    test_osdvd)   build "$t" test_osdvd $PLAT_CORE ;;
    *) echo "cible inconnue : $t"; exit 1 ;;
    esac
done

"$ADB" shell mkdir -p "$DEV"
for t in $TARGETS; do
    "$ADB" push "$OUT/$t" "$DEV/$t" >/dev/null
    "$ADB" shell chmod 755 "$DEV/$t"
done
echo "pousse dans $DEV : $TARGETS"
