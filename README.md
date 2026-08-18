# Resident Evil 4 — Native Port & Matching Decompilation (GameCube)

[![License: CC0-1.0](https://img.shields.io/badge/License-CC0_1.0-lightgrey.svg)](https://creativecommons.org/publicdomain/zero/1.0/)
[![Discord](https://img.shields.io/discord/128500000000000000?color=7289da&label=Discord&logo=discord&logoColor=white)](https://discord.gg/AXAzExECSv)
[![Platform: Windows | Android | iOS](https://img.shields.io/badge/Platform-Windows%20%7C%20Android%20%7C%20iOS-blue.svg)]()

A complete, 100% native source port and byte-identical decompilation of **Resident Evil 4 (Nintendo GameCube, G4BE08)** for modern platforms: **Windows**, **Android**, and **iOS**.

There is no emulator: the game code compiles directly to native x86_64, ARM32, and ARM64 machine instructions, communicating with modern graphics hardware through an OpenGL ES 3.0 / Metal translation layer.

> [!IMPORTANT]
> **Bring Your Own Game (BYOG)**: This repository contains **NO game assets, audio, textures, 3D models, or copyrighted binaries**. You must provide your own legally dumped GameCube disc image (`.iso`, `.gcm`, or `.rvz`) to play. The built-in launcher verifies and extracts the required data locally on your device.

---

## Features

- **100% Native Performance**: Runs natively on modern CPUs with zero emulation overhead, locked at a stable 30 FPS with authentic GameCube frame pacing.
- **Cross-Platform**:
  - **Windows (x86_64)**: Full keyboard/mouse and gamepad support.
  - **Android (ARM64-v8a, armeabi-v7a, x86_64)**: High performance, tested across modern ARM64 devices and older flagships (including Snapdragon 801 / Galaxy S5).
  - **iOS (ARM64, iOS 14.0+)**: Native 64-bit app with direct CAEAGLLayer display pipeline.
- **Widescreen & Enhancements**:
  - 16:9 anamorphic widescreen presentation.
  - Optional Wii Edition pointer-style free aiming with mouse or analog stick.
- **Adaptive Touch Controls (Mobile)**:
  - Context-aware virtual HUD adapting dynamically between exploration, weapon aiming, inventory management, quick-time events (QTEs), and cutscenes.
- **Typewriter Save System**:
  - Native virtual Memory Card storage (.raw) compatible with typewriter saving and loading.
- **Offline Launcher**:
  - Standalone verification and installation directly from your legal disc dump. No mandatory online accounts or persistent connection required.

---

## Repository Structure

The project is structured as a unified monorepo encompassing both the native engine runtime and the matching decompilation codebase:

```
Resident-Evil-4-Native-Port/
  ├── src/          # Decompiled game logic (992 matching translation units)
  ├── include/      # Game and hardware interface headers
  ├── port/         # Native platform abstraction (OS, DVD, VI, Audio, GX translation)
  ├── runtime/      # Engine runtime (ImGui launcher, audio mixer, renderer backend)
  ├── android/      # Android packaging, JNI bindings, and touch HUD
  ├── ios/          # iOS application bundle and Metal/EAGL display integration
  ├── recomp/       # Static analysis and symbol translation utilities
  ├── tools/        # Matching verification scripts, symbol extractors
  └── docs/         # Architecture notes, research logs, and hardware documentation
```

---

## Quick Start (Playing)

1. **Download or Build** the release package for your platform (Windows executable, Android APK, or iOS IPA).
2. **Launch the Application**: The launcher interface will appear.
3. **Select Your Disc**: Point the launcher to your legal GameCube disc image of *Resident Evil 4* (Disc 1; Disc 2 optional for the island chapter).
4. **Play**: The launcher verifies the disc integrity, prepares the local game directory, and starts the game natively.

---

## Building from Source

Detailed build guides for all supported platforms are documented in [BUILDING.md](BUILDING.md).

### Summary:

- **Windows**: Requires Visual Studio 2022 (or Clang) and CMake 3.20+.
  ```sh
  cmake -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release
  ```
- **Android**: Requires Android NDK r26d and CMake.
  ```sh
  ./android/build_apk.sh arm64-v8a
  ```
- **iOS**: Requires Xcode 14+ on macOS.
  ```sh
  cd ios && xcodebuild -project RE4Native.xcodeproj -configuration Release -sdk iphoneos
  ```
- **GameCube Matching DOL/REL**:
  ```sh
  python3 configure.py && ninja
  ```

---

## Legal & Compliance

Please review [LEGAL.md](LEGAL.md) for full compliance details.

- *Resident Evil* and *Biohazard* are registered trademarks of **Capcom Co., Ltd.**
- *Nintendo GameCube* is a registered trademark of **Nintendo Co., Ltd.**
- This project is an independent fan-made preservation effort and is **not affiliated with, endorsed by, or sponsored by Capcom or Nintendo**.
- All game assets must be legally sourced by the end user from their own original retail game media.

---

## Community & Support

- **Discord**: Join our community for help, bug reports, and discussion: [discord.gg/AXAzExECSv](https://discord.gg/AXAzExECSv)
- **Issues**: Report bugs or technical issues via GitHub Issues. Please do not attach copyrighted game files or ROMs.
