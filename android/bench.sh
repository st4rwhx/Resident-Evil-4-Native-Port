#!/usr/bin/env bash
# Mesure comparable d'un APK sur le Galaxy S5 : attend que le telephone refroidisse, va en jeu,
# puis releve [PERF] guest_mips et la frequence du coeur 0 pendant 60 s.
#
#   android/bench.sh [-i] [etiquette]
#
# Pourquoi : le 25/09, des lancements enchaines ont chauffe le S5 a 56 degres ; le thermique
# coupe alors 2 coeurs sur 4 et plafonne les autres a 1,19 GHz au lieu de 2,46. Un meme build
# passait de 25 a 15 millions d'instructions par seconde, plus que l'effet des optimisations
# mesurees. Un chiffre sans la temperature de depart et la frequence ne vaut rien.
set -u
export MSYS_NO_PATHCONV=1
ADB=${ADB:-/c/android/sdk/platform-tools/adb.exe}
HERE="$(cd "$(dirname "$0")" && pwd)"
COOL=${COOL:-45}  # le S5 reste a ~43 degres au repos
inst=()
if [ "${1:-}" = "-i" ]; then inst=(-i); shift; fi
label=${1:-sans-nom}

hot() { "$ADB" shell 'cat /sys/class/thermal/thermal_zone*/type /sys/class/thermal/thermal_zone*/temp' \
        | tr -d '\r' | awk '/^tsens/{n++} /^[0-9]+$/{t[++m]=$1} END{mx=0; for(i=1;i<=m;i++) if(t[i]<200 && t[i]>mx) mx=t[i]; print mx}'; }

"$ADB" shell am force-stop com.ayano.nightjar
while :; do
    t=$(hot)
    [ "$t" -le "$COOL" ] && break
    echo "  refroidissement : $t degres (cible $COOL)"
    sleep 30
done
echo "depart a $(hot) degres"
"$HERE/run_to_game.sh" "${inst[@]}" || exit 1

freqs=()
for _ in $(seq 1 12); do
    sleep 5
    freqs+=("$("$ADB" shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq | tr -d '\r')")
done
mips=$("$ADB" logcat -d -s NJ | grep "\[PERF\]" | grep -o "guest_mips=[0-9.]*" | cut -d= -f2 | tail -12)
frames=$("$ADB" logcat -d -s NJ | grep "\[STAT\]" | grep -o "frames=[0-9]*" | cut -d= -f2 | tail -13)
med=$(sort -n <<<"$mips" | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
fps=$(awk 'NR==1{f=$1} {l=$1} END{printf "%.2f", (l-f)/((NR-1)*5)}' <<<"$frames")
fmed=$(printf "%s\n" "${freqs[@]}" | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
line="$(date +%F_%T) $label mips_med=$med fps=$fps cpu0_khz_med=$fmed fin=$(hot)C"
echo "$line"
echo "$line" >> "$HERE/bench_results.txt"
