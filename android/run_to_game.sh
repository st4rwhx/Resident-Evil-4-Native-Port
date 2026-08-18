#!/usr/bin/env bash
# Installe (option -i) et lance RE4 sur le telephone jusqu'a la salle de depart, puis rend la main.
#
#   android/run_to_game.sh [-i] [NJ_X=valeur ...]
#
# Le menu de debug attend un appui sur A : on tape au milieu de l'ecran une fois qu'il est affiche
# (~50 s sur le Galaxy S5). Taper plus tot tombe sur l'ecran titre, et « Charger » y bloque le jeu
# (carte memoire inachevee). L'arrivee en jeu se lit dans le journal [CTX] : Task[0].func =
# 0x80142EC0 (voir nWiiRuntime/src/input/touch/re4_context.cpp).
set -u
export MSYS_NO_PATHCONV=1
ADB=${ADB:-/c/android/sdk/platform-tools/adb.exe}
APK=${APK:-C:/re4n/apk/RE4-Native.apk}
PKG=com.ayano.nightjar

if [ "${1:-}" = "-i" ]; then
    shift
    # adb rend 0 meme quand l'installation echoue (telephone plein, 25/09) : lire le verdict.
    out=$("$ADB" install -r "$APK" 2>&1)
    if ! grep -q "^Success" <<<"$out"; then
        echo "installation refusee :" >&2
        grep -m3 "Exception\|Failure\|failed" <<<"$out" >&2
        exit 2
    fi
fi
extras=(--es NJ_BOOTDIRECT 1 --es NJ_AUTOSTART 150 --es NJ_CTX_LOG 1)
for kv in "$@"; do
    extras+=(--es "${kv%%=*}" "${kv#*=}")
done

"$ADB" shell am force-stop $PKG
"$ADB" logcat -c
"$ADB" shell am start -n $PKG/org.nightjar.re4.RE4Activity "${extras[@]}" | tail -1
# Nightjar : l'AutoStart traverse lui-meme facade, carte et menu de debug, et se tait quand Leon
# est jouable depuis 2 s (autostart_source.cpp). Plus d'appui sur l'ecran : un contact tactile
# fait taire l'AutoStart (il laisse la main au joueur), et un appui a heure fixe tombait sur
# l'ecran titre des que le rendu allait plus vite (la video d'attente se lancait).
for _ in $(seq 1 100); do
    sleep 3
    if "$ADB" logcat -d -s NJ | grep -q "jeu jouable"; then
        echo "en jeu"
        exit 0
    fi
done
echo "pas en jeu apres 300 s" >&2
exit 1
