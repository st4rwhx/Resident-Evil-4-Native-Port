#!/usr/bin/env bash
# Frequence fixe pour mesurer (S5, adb root requis : Options developpeurs -> Acces root -> ADB).
#   android/freq.sh fixe [kHz]   4 coeurs en ligne, gouverneur performance, min = max = kHz (1728000)
#   android/freq.sh libre        remet mpdecision et le gouverneur interactive
#   android/freq.sh etat         coeurs, frequences, temperature
# Les protections thermiques (thermal-engine, msm_thermal) restent actives : si elles brident,
# `etat` le montre et la mesure est a jeter. Un redemarrage du telephone annule tout.
set -u
export MSYS_NO_PATHCONV=1
ADB=${ADB:-/c/android/sdk/platform-tools/adb.exe}
K=${2:-1728000}
case "${1:-etat}" in
fixe)
    "$ADB" shell "stop mpdecision; for c in 0 1 2 3; do echo 1 > /sys/devices/system/cpu/cpu\$c/online; done; sleep 1;
      for c in 0 1 2 3; do f=/sys/devices/system/cpu/cpu\$c/cpufreq; echo performance > \$f/scaling_governor;
      echo $K > \$f/scaling_max_freq; echo $K > \$f/scaling_min_freq; done" ;;
libre)
    "$ADB" shell "for c in 0 1 2 3; do f=/sys/devices/system/cpu/cpu\$c/cpufreq; echo interactive > \$f/scaling_governor 2>/dev/null;
      echo 300000 > \$f/scaling_min_freq 2>/dev/null; echo 2457600 > \$f/scaling_max_freq 2>/dev/null; done; start mpdecision" ;;
esac
"$ADB" shell "echo coeurs \$(cat /sys/devices/system/cpu/online); for c in 0 1 2 3; do echo cpu\$c \$(cat /sys/devices/system/cpu/cpu\$c/cpufreq/scaling_cur_freq 2>/dev/null); done;
  echo temp \$(cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | awk '\$1<200 && \$1>m {m=\$1} END{print m}')"
