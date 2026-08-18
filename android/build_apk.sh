#!/usr/bin/env bash
# Construit l'APK sans Gradle : cmake (NDK) -> javac -> d8 -> aapt2 -> zipalign -> apksigner.
#
#   android/build_apk.sh [abi...]       abi : arm64-v8a (defaut), armeabi-v7a, x86_64
#   LIBS_ONLY=1 / PREBUILT=1             compiler les .so ailleurs / empaqueter des .so deja compiles
#   RELEASE=1 android/build_apk.sh ...  build de diffusion : lanceur obligatoire (NJ_RELEASE),
#                                       application non debogable, dossier de build a part, aucun
#                                       interrupteur NJ_ ni journal (logcat), symboles retires,
#                                       signee avec la cle de diffusion (release.keystore)
#
# Chaque ABI se construit dans un chemin sans espace.
# Sortie : C:/re4n/apk/Nightjar.apk, signee avec une cle de debug creee au premier passage.
set -euo pipefail
if [ -n "${JAVA_HOME:-}" ]; then export PATH="$JAVA_HOME/bin:$PATH";
elif command -v javac >/dev/null 2>&1; then :;
elif [ -d '/c/Program Files/Eclipse Adoptium/jdk-21.0.11.10-hotspot' ]; then export JAVA_HOME='C:/Program Files/Eclipse Adoptium/jdk-21.0.11.10-hotspot'; export PATH="$JAVA_HOME/bin:$PATH"; fi
export MSYS_NO_PATHCONV=1

HERE="$(cd "$(dirname "$0")" && pwd)"
SDK=${ANDROID_HOME:-C:/android/sdk}
NDK=${ANDROID_NDK:-C:/android/android-ndk-r26d}
BT=$SDK/build-tools/35.0.0
PLATFORM=$SDK/platforms/android-34/android.jar
EXPORT=${NJ_EXPORT_DIR:-C:/re4n/export}
OUT=C:/re4n/apk
ABIS=("$@"); [ ${#ABIS[@]} -eq 0 ] && ABIS=(arm64-v8a)
NINJA=$(cygpath -m "$(command -v ninja)")
STRIP=$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe

mkdir -p "$OUT/lib" "$OUT/classes" "$OUT/dex"
rm -rf "$OUT/lib"/* "$OUT/classes"/* "$OUT/dex"/*

for abi in "${ABIS[@]}"; do
    # NJ_SUFFIX : dossier de build a part (ex. -nj pour Nightjar, sources dans NJ_EXPORT_DIR).
    B=C:/re4n/android-$abi${RELEASE:+-release}${NJ_SUFFIX:-}
    if [ -n "${PREBUILT:-}" ]; then
        # bibliotheque compilee ailleurs (portable), copiee dans $B : seulement la retirer et l'empaqueter
        mkdir -p "$OUT/lib/$abi"
        "$STRIP" $([ -n "${RELEASE:-}" ] && [ -z "${NJ_DEVTOOLS:-}" ] && echo --strip-unneeded || echo --strip-debug) -o "$OUT/lib/$abi/libmain.so" "$B/libmain.so"
        [ -n "${RELEASE:-}" ] && python "$(cygpath -m "$HERE/scrub_strings.py")" "$OUT/lib/$abi/libmain.so"
        continue
    fi
    if [ ! -f "$B/build.ninja" ]; then
        cmake -S "$(cygpath -m "$HERE")" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
            -DANDROID_ABI=$abi -DANDROID_PLATFORM=android-24 \
            -DNJ_EXPORT_DIR=$EXPORT -DCMAKE_MAKE_PROGRAM="$NINJA" \
            -DNJ_RELEASE=$([ -n "${RELEASE:-}" ] && echo ON || echo OFF) \
            ${NJ_NATIVE_LIBS:+-DNJ_NATIVE_ONLY=ON} \
            ${NJ_PRECOMPILED_MULTI:+-DNJ_PRECOMPILED_MULTI=$NJ_PRECOMPILED_MULTI} \
            ${NJ_USA_DIR:+-DNJ_USA_DIR=$NJ_USA_DIR} ${NJ_EUR_DIR:+-DNJ_EUR_DIR=$NJ_EUR_DIR} \
            ${NJ_DEPS_DIR:+-DNJ_DEPS_DIR=$NJ_DEPS_DIR} ${NJ_DEVTOOLS:+-DNJ_DEVTOOLS=ON} \
            ${P3:+-DNJ_P3_JIT=ON -DLLVM_DIR=C:/re4n/llvm/build-android-$abi/lib/cmake/llvm -DClang_DIR=C:/re4n/llvm/build-android-$abi/lib/cmake/clang -DNJ_P3_PROFILS=${NJ_P3_PROFILS:-C:/re4n/p3profils-android}}
    fi
    # diffusion : traducteur et connexion Discord brouilles par Polaris (13/10)
    [ -n "${RELEASE:-}" ] && cmake -S "$(cygpath -m "$HERE")" -B "$B" -DNJ_OBFUSCATION=ON > /dev/null
    # outils de dev prives (visite des salles, enregistreur d'entrees) : toujours reconfigure, ON seulement si NJ_DEVTOOLS=1 (le cache gardait OFF)
    cmake -S "$(cygpath -m "$HERE")" -B "$B" -DNJ_DEVTOOLS=$([ -n "${NJ_DEVTOOLS:-}" ] && echo ON || echo OFF) > /dev/null
    cmake --build "$B" -j${JOBS:-2} --target main
    mkdir -p "$OUT/lib/$abi"
    "$STRIP" $([ -n "${RELEASE:-}" ] && [ -z "${NJ_DEVTOOLS:-}" ] && echo --strip-unneeded || echo --strip-debug) -o "$OUT/lib/$abi/libmain.so" "$B/libmain.so"
        [ -n "${RELEASE:-}" ] && python "$(cygpath -m "$HERE/scrub_strings.py")" "$OUT/lib/$abi/libmain.so"
done

# LIBS_ONLY=1 : compiler les bibliotheques seulement (machine de build) ; l'APK est assemble et
# signe sur la tour (PREBUILT=1), ou reste la cle de diffusion.
[ -n "${LIBS_ONLY:-}" ] && exit 0

# Moteur natif (variante) : NJ_NATIVE_LIBS = dossier du noyau compile (libre4_*.so + modules/), v7a seulement.
# Les bibliotheques du noyau, reduites une fois, restent dans NJ_NATIVE_CACHE (le strip de 117 fichiers est lent).
if [ -n "${NJ_NATIVE_LIBS:-}" ]; then
    D="$OUT/lib/armeabi-v7a"; mkdir -p "$D"
    C="${NJ_NATIVE_CACHE:-C:/re4n/natif_cache}"; mkdir -p "$C"
    for f in "$NJ_NATIVE_LIBS"/libre4_*.so "$NJ_NATIVE_LIBS"/modules/libre4_*.so; do
        [ "$f" -nt "$C/$(basename "$f")" ] && "$STRIP" --strip-unneeded -o "$C/$(basename "$f")" "$f"
        cp "$C/$(basename "$f")" "$D/"
    done
    cp "$NDK/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/arm-linux-androideabi/libc++_shared.so" "$D/"
fi

# Coeur natif Nightjar (notre portage) : NJ_CORE_EXE = le programme du coeur (re4-decomp-upstream/_port_build/re4),
# range comme bibliotheque libnjcore.so : Android n'execute que depuis le dossier des bibliotheques (extractNativeLibs).
if [ -n "${NJ_CORE_EXE:-}" ]; then
    # ABI du coeur = la premiere demandee (armeabi-v7a pour le telephone, x86 pour le banc G0 dans MuMu, 14/11).
    D="$OUT/lib/${ABIS[0]}"; mkdir -p "$D"
    "$STRIP" --strip-unneeded -o "$D/libnjcore.so" "$NJ_CORE_EXE"
fi

# Java : les sources de SDL (celles de la version exacte compilee) plus notre activite. Un chemin
# entre guillemets par ligne dans les fichiers reponse si le chemin contient un espace.
SDLJAVA="${NJ_DEPS_DIR:-$EXPORT/build/_deps}/sdl2-src/android-project/app/src/main/java"
# Diffusion : une copie de nos sources avec RE4Activity.DEV = false.
JAVA="$HERE/java"
if [ -n "${RELEASE:-}" ]; then
    RJ="$(cygpath -u "$OUT/java_release")"
    rm -rf "$RJ"
    cp -r "$HERE/java" "$RJ"
    sed -i 's/static final boolean DEV = true;/static final boolean DEV = false;/' \
        "$RJ/org/nightjar/re4/RE4Activity.java"
    grep -q "DEV = false" "$RJ/org/nightjar/re4/RE4Activity.java"
    JAVA="$RJ"
fi
find "$(cygpath -u "$SDLJAVA")" "$JAVA" -name '*.java' \
    | while read -r f; do echo "\"$(cygpath -m "$f")\""; done > "$OUT/sources.txt"
javac -J-Xmx256m -J-XX:TieredStopAtLevel=1 -encoding UTF-8 -nowarn --release 11 -classpath "$PLATFORM" \
    -d "$OUT/classes" @"$OUT/sources.txt" 2>&1 | grep -v "^warning\|^Note:\|^[0-9]* warning" || true
find "$(cygpath -u "$OUT/classes")" -name '*.class' | while read -r f; do cygpath -m "$f"; done > "$OUT/classes.txt"
"$BT/d8.bat" --min-api 24 --lib "$PLATFORM" --output "$OUT/dex" @"$OUT/classes.txt"

MANIFEST="$OUT/AndroidManifest.xml"
if [ -n "${RELEASE:-}" ]; then
    sed 's/android:debuggable="true"/android:debuggable="false"/' "$HERE/AndroidManifest.xml" > "$MANIFEST"
else
    cp "$HERE/AndroidManifest.xml" "$MANIFEST"
fi
# Essais : NJ_APP_ID=<id> installe a cote de la vraie application (autre paquet, autre cle).
[ -n "${NJ_APP_ID:-}" ] && sed -i "s|com.ayano.nightjar|$NJ_APP_ID|g" "$MANIFEST"
# Ressources (icone Nightjar, tools/nightjar_icon.py) : compilees puis liees avec le manifeste.
rm -f "$OUT/res.zip"
"$BT/aapt2.exe" compile --dir "$(cygpath -m "$HERE/res")" -o "$OUT/res.zip"
# assets/ : invites tactiles des boutons (textures_hd/buttons/mobile, tools/re4/hud_buttons.py), copiees
# dans le dossier de l'application au premier lancement (RE4Activity.installAssets).
"$BT/aapt2.exe" link -o "$OUT/base.apk" -I "$PLATFORM" --manifest "$MANIFEST" "$OUT/res.zip" \
    -A "$(cygpath -m "$HERE/assets")" \
    --min-sdk-version 24 --target-sdk-version 34
python "$(cygpath -m "$HERE/apk_add.py")" "$OUT/base.apk" "$OUT/unsigned.apk" "$OUT"

# Cle : de debug pour les essais ; pour la diffusion, une cle a part (release.keystore) et son mot de
# passe aleatoire (release.pass), crees au premier passage, qui ne quittent jamais cette machine.
# A GARDER PRECIEUSEMENT : Android refuse une mise a jour signee avec une autre cle.
if [ -n "${RELEASE:-}" ]; then
    KS=C:/re4n/apk/release.keystore
    PASSF="$(cygpath -u C:/re4n/apk/release.pass)"
    if [ ! -f "$(cygpath -u "$KS")" ]; then
        python -c "import secrets; print(secrets.token_urlsafe(24))" > "$PASSF"
        P=$(cat "$PASSF")
        keytool -genkeypair -keystore "$KS" -storepass "$P" -keypass "$P" -alias re4native \
            -keyalg RSA -keysize 4096 -validity 20000 -dname "CN=Ayano" >/dev/null 2>&1
    fi
    P=$(cat "$PASSF")
else
    KS=C:/re4n/apk/debug.keystore
    P=android
    [ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass android -keypass android \
        -alias debug -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Debug" >/dev/null 2>&1
fi
"$BT/zipalign.exe" -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
"$BT/apksigner.bat" sign --ks "$KS" --ks-pass pass:"$P" --out "$OUT/Nightjar.apk" "$OUT/aligned.apk"
ls -la "$(cygpath -u "$OUT/Nightjar.apk")"
