# Building Resident Evil 4 Native Port

This document details how to compile the native game and the decompilation verification target on supported operating systems.

---

## 1. Prerequisites

### Common Tools
- [CMake 3.20+](https://cmake.org/download/)
- [Git](https://git-scm.com/)
- [Python 3.9+](https://www.python.org/)

---

## 2. Windows Build (PC)

### Toolchain Requirements
- Visual Studio 2022 (with "Desktop development with C++") or LLVM Clang.
- Windows 10/11 64-bit.

### Build Steps

1. Configure with CMake:
   ```cmd
   cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
   ```
2. Build the runtime and game core:
   ```cmd
   cmake --build build --config Release --parallel
   ```
3. The resulting binary will be placed in `build/Release/RE4Native.exe`.

---

## 3. Android Build (APK)

### Toolchain Requirements
- [Android NDK r26d](https://developer.android.com/ndk/downloads)
- [Android SDK Platform 34](https://developer.android.com/studio)
- JDK 17 or JDK 21

### Build Steps

Set your environment variables pointing to your SDK and NDK:
```bash
export ANDROID_HOME="/path/to/android/sdk"
export ANDROID_NDK="/path/to/android-ndk-r26d"
```

To build an ARM64-v8a package (standard for modern Android devices):
```bash
./android/build_apk.sh arm64-v8a
```

To build for 32-bit ARM (older devices such as Galaxy S5):
```bash
./android/build_apk.sh armeabi-v7a
```

The compiled APK will be generated at `apk/RE4-Native.apk`.

---

## 4. iOS Build (IPA)

### Toolchain Requirements
- macOS Ventura or Sonoma
- Xcode 14.0 or newer
- iOS 14.0+ SDK

### Build Steps

1. Open the project in Xcode:
   ```bash
   open ios/RE4Native.xcodeproj
   ```
2. Select your signing team under the `Signing & Capabilities` tab.
3. Build for an iOS device target or archive to produce an `.ipa` bundle:
   ```bash
   xcodebuild -project ios/RE4Native.xcodeproj -scheme RE4Native -configuration Release -sdk iphoneos -archivePath build/RE4Native.xcarchive archive
   ```

---

## 5. GameCube Matching Decompilation (Byte-Identical Verification)

To verify the matching status of the decompiled C/C++ units against the original GameCube debug DOL:

1. Place your legally dumped original debug disc ISOs in `orig/G4BE08/`:
   - `orig/G4BE08/re4_debug_disc1.iso`
   - `orig/G4BE08/re4_debug_disc2.iso`
2. Run configuration and ninja:
   ```bash
   python3 configure.py
   ninja
   ```
3. Ninja will output matching statistics for each module and verify SHA-1 checksums against `config/G4BE08/build.sha1`.
