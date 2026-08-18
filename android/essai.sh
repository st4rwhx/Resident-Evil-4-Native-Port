#!/usr/bin/env bash
# Essai rapide sur le telephone : lance le jeu jusqu'en jeu avec des interrupteurs, puis releve le
# chronometrage du rendu ([RPROF]) et l'etat du telephone.
#
#   android/essai.sh [-i] [NJ_X=valeur ...]
#
# NJ_RPROF=${RPROF:-2} est toujours ajoute. APK=<chemin> : l'APK a installer avec -i.
#
# Conditions de mesure (29/09) : le S5 chaud ou a faible batterie coupe deux coeurs sur quatre et
# bride la frequence ; deux mesures ont ainsi fait croire a une regression de 3x. Avant de lancer,
# on attend : 4 coeurs en ligne, temperature <= TEMP_MAX (defaut 52), batterie >= BATT_MIN
# (defaut 35). ETAT_MAX_S (defaut 1800) : au-dela, on lance quand meme en le signalant.
set -u
export MSYS_NO_PATHCONV=1
ADB=${ADB:-/c/android/sdk/platform-tools/adb.exe}
HERE="$(cd "$(dirname "$0")" && pwd)"

etat() {
    local on t b
    on=$("$ADB" shell cat /sys/devices/system/cpu/online | tr -d '\r')
    t=$("$ADB" shell 'cat /sys/class/thermal/thermal_zone*/temp' | tr -d '\r' | awk '$1<200 && $1>m {m=$1} END{print m}')
    b=$("$ADB" shell dumpsys battery | tr -d '\r' | awk '/level:/ {print $2; exit}')
    echo "$on $t $b"
}
"$ADB" shell am force-stop com.ayano.nightjar
debut=$(date +%s)
while :; do
    read -r on t b <<<"$(etat)"
    # Au repos le noyau met les coeurs 2-3 hors ligne (hotplug) : on ne juge les coeurs qu'en jeu (fin).
    if [ "${t:-99}" -le "${TEMP_MAX:-52}" ] && [ "${b:-0}" -ge "${BATT_MIN:-35}" ]; then
        break
    fi
    if [ $(( $(date +%s) - debut )) -ge "${ETAT_MAX_S:-1800}" ]; then
        echo "  ATTENTION : conditions non reunies (coeurs $on, $t degres, batterie $b %), essai lance quand meme"
        break
    fi
    echo "  attente : coeurs $on, $t degres, batterie $b %"
    sleep 30
done
# FREQ=<kHz> : frequence fixe (freq.sh, adb root) pendant l'essai seulement ; l'attente de
# refroidissement se fait en frequence libre (fixee, le telephone ne refroidit pas au repos).
[ -n "${FREQ:-}" ] && bash "$HERE/freq.sh" fixe "$FREQ" > /dev/null
read -r on0 t0 b0 <<<"$(etat)"
ADB="$ADB" bash "$HERE/run_to_game.sh" "$@" NJ_RPROF=${RPROF:-2} >/dev/null || { echo "pas en jeu"; exit 1; }
# Une video (Sofdec) en cours : la passer d'un appui (le jeu va plus loin quand le rendu est rapide).
for _ in 1 2 3 4 5 6; do
    "$ADB" logcat -d -s NJ | grep CTX | tail -1 | grep -q "sofdec=1 " || break
    "$ADB" shell input tap 960 540
    sleep 4
done
# Attendre la scene (step=3 dans [CTX]) : l'AutoStart se tait deja pendant le chargement.
for _ in $(seq 1 30); do
    "$ADB" logcat -d -s NJ | grep CTX | tail -1 | grep -q "step=3" && break
    sleep 5
done
sleep "${ATTENTE:-16}"
read -r on1 t1 b1 <<<"$(etat)"
f=$("$ADB" shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq | tr -d '\r')
[ -n "${FREQ:-}" ] && bash "$HERE/freq.sh" libre > /dev/null
echo "[$*] depart : coeurs $on0, $t0 degres, batterie $b0 % ; fin : coeurs $on1, $t1 degres, coeur0 ${f} kHz"
"$ADB" logcat -d -s NJ | grep RPROF | tail -2 | sed 's/.*\[RPROF\]/  /'
