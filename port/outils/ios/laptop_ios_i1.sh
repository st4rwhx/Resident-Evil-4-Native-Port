#!/bin/bash
# I1 (18/11) : chaine iOS locale sans LLVM 20 : clang du NDK r26d + vrai SDK iPhoneOS + ld64.lld (LLVM 18). Test C++ + Objective-C + lien.
H=user@laptop
ssh -o ServerAliveInterval=15 $H 'bash -s' <<'EOS'
B=C:/re4n/llvm/build-win64/bin   # clang 23.1.2, notre build de LLVM (lecture seule, ne rien y ecrire)
SDK=C:/re4n/ios_sdk/iPhoneOS.sdk
LLD=C:/re4n/lld18/bin/ld64.lld.exe
T=/c/re4n/ios_i1; rm -rf $T; mkdir -p $T; cd $T
printf '#include <cstdio>\n#include <string>\n#include <thread>\n#include <sys/mman.h>\nextern "C" int test_cpp(){ std::string s("ok"); std::thread t([]{}); t.join(); void*p=mmap(nullptr,1<<20,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0); std::printf("%%s %%p\\n", s.c_str(), p); return 0; }\n' > a.cpp
printf '#import <Foundation/Foundation.h>\nint test_cpp(void);\nint main(void){ @autoreleasepool { NSLog(@"objc ok"); } return test_cpp(); }\n' > m.m
F="--target=arm64-apple-ios16.0 -isysroot $SDK -miphoneos-version-min=16.0"
$B/clang++.exe $F -std=c++20 -c a.cpp -o a.o > a.log 2>&1 && echo "C++ : OK" || { echo "C++ : ECHEC"; head -8 a.log; }
$B/clang.exe $F -fobjc-arc -c m.m -o m.o > m.log 2>&1 && echo "ObjC : OK" || { echo "ObjC : ECHEC"; head -8 m.log; }
$B/clang++.exe $F --ld-path=$LLD -fuse-ld=lld -Wl,-arch,arm64 -Wl,-platform_version,ios,16.0,18.5 -Wl,-syslibroot,$SDK a.o m.o -framework Foundation -o test_i1 > l.log 2>&1 && echo "lien : OK" || { echo "lien : ECHEC"; head -10 l.log; }
[ -f test_i1 ] && od -A x -t x1 -N 8 test_i1 | head -1
EOS
