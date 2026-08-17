#!/bin/bash
# Port natif complet iOS (18/11, I6-I7) : IPA construite SUR LE PORTABLE, sans GitHub ni Mac.
#   - coeur : laptop_ios_natif.sh (build_game + build_rel, PORT_ABI=ios) puis port/outils/macho_lien.py -> libnjnatif.a
#   - moteur : nj-m1 (actuel) en mode NJ_NATIVE_CORE (ios/CMakeLists.txt), clang 23 (notre build de LLVM) + SDK iPhoneOS
#     + ld64.lld (LLVM 18) ; chaine d'outils toolchain_natif.cmake (copie de toolchain_laptop.cmake, LLVM 20 absent)
#   -> C:/re4n/ios_ipa/Nightjar-natif.ipa (SideStore + LiveContainer)
set -e
H=admin@192.168.1.124
M="$(dirname "$0")"
bash "$M/laptop_ios_natif.sh" 2>&1 | tail -3
ssh $H "mkdir -p /c/re4n/nj-local-natif/src"
tar cf - -C C:/re4n/nj-m1 runtime ios recomp tools | ssh $H "cd /c/re4n/nj-local-natif/src && tar xmf -"
ssh -o ServerAliveInterval=15 $H 'bash -s' <<'EOS'
set -e
export PATH="/c/Program Files/CMake/bin:/c/Users/Admin/AppData/Local/Microsoft/WinGet/Links:$PATH"
cd /c/re4n/nj-local-natif
[ -d src/p3profils ] || cp -r /c/re4n/nj-local/src/p3profils src/ 2>/dev/null || true
sed -e 's#C:/re4n/llvm20/bin/ld64.lld.exe#C:/re4n/lld18/bin/ld64.lld.exe#' \
    -e 's#set(NJ_NDK  "C:/re4n/llvm20"#set(NJ_NDK  "C:/re4n/llvm/build-win64"#' \
    -e 's#"${NJ_NDK}/bin/llvm-ar.exe"#"C:/android/android-ndk-r26d/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ar.exe"#' \
    -e 's#"${NJ_NDK}/bin/llvm-ranlib.exe"#"C:/android/android-ndk-r26d/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ranlib.exe"#' \
    /c/re4n/ios_local/toolchain_laptop.cmake > /c/re4n/ios_local/toolchain_natif.cmake
cmake -S src/ios -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/re4n/ios_local/toolchain_natif.cmake \
  -DNJ_NATIVE_CORE=ON -DNJ_NATIVE_LIB=C:/re4n/re4port/_port_build_ios/lien/libnjnatif.a -DNJ_NATIVE_SRC=C:/re4n/re4port \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=C:/re4n/nj-local/build/_deps/sdl2-src \
  -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=C:/re4n/nj-local/build/_deps/tomlplusplus-src \
  "-DCMAKE_EXE_LINKER_FLAGS=--ld-path=C:/re4n/lld18/bin/ld64.lld.exe --target=arm64-apple-ios16.0 -isysroot C:/re4n/ios_sdk/iPhoneOS.sdk -Wl,-arch,arm64 -Wl,-platform_version,ios,16.0,18.5 -Wl,-syslibroot,C:/re4n/ios_sdk/iPhoneOS.sdk -Wl,-lSystem -Wl,-lc++ -Wl,-lobjc" > cfg.log 2>&1 || { echo CONFIG_ECHEC; grep -E "Error|error" cfg.log | head -12; exit 1; }
ninja -C build njruntime > rt.log 2>&1 || { echo MOTEUR_ECHEC; grep -E 'error:|FAILED' rt.log | head -10; exit 1; }
( cd /c/re4n/re4port && rm -f _port_build_ios/rel/obj/Tools__* _port_build_ios/rel/obj/t_* && /c/Python314/python port/outils/macho_lien.py --engine C:/re4n/nj-local-natif/build/runtime/libnjruntime.a 2>&1 | tail -6 )
ninja -C build Nightjar > build.log 2>&1 || { echo BUILD_ECHEC; grep -E "error:|FAILED|undefined symbol|duplicate symbol" build.log | sort | uniq -c | sort -rn | head -25 | cut -c1-230; exit 1; }
rm -rf ipa/Payload && mkdir -p ipa/Payload && cp -R build/Nightjar.app ipa/Payload/
cd ipa && rm -f ../Nightjar-natif.ipa
/c/Python314/python -I -c "
import zipfile,os
z=zipfile.ZipFile('../Nightjar-natif.ipa','w',zipfile.ZIP_DEFLATED)
for d,ds,fs in os.walk('Payload'):
    z.write(d, d.replace(os.sep,'/')+'/')
    for f in fs: z.write(os.path.join(d,f), os.path.join(d,f).replace(os.sep,'/'))
z.close()"
ls -la ../Nightjar-natif.ipa
EOS
mkdir -p C:/re4n/ios_ipa
ssh $H "cat /c/re4n/nj-local-natif/Nightjar-natif.ipa" > C:/re4n/ios_ipa/Nightjar-natif.ipa
python -I -c "import zipfile;z=zipfile.ZipFile(r'C:/re4n/ios_ipa/Nightjar-natif.ipa');print('zip intact:',z.testzip() is None, len(z.namelist()),'fichiers')"
ls -la C:/re4n/ios_ipa/Nightjar-natif.ipa
