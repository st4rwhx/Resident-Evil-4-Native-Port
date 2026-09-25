# Resident Evil 4 — Native Port (GameCube)

A native port of **Resident Evil 4 for the Nintendo GameCube** to **Windows** and **Android**.
There's no emulator: the game runs directly on your PC or phone.

> **No game data is included, and none ever will be. Bring your own disc.**
> The project is **closed source**. This repository hosts the releases, the issue tracker
> and the project news.

![Running natively on a Galaxy S5](images/android_s5.png)
*Android, on a 2014 Samsung Galaxy S5, in the village, with touch controls. The text overlay is
the debug build's own HUD.*

---

## Status

| platform | state |
|---|---|
| **Windows** | Playable: village, textures, lighting, HUD, sound in game. ~20 fps. No music, FMV or saves yet |
| **Android** | Boots and draws the village on a real phone, with full touch controls. Too slow to play for now (~1-2 fps) |
| **iOS** | Planned, after Android |

Target build: the **Nov 25 2004 debug build** (G4BE08).

## How you will play it

When the first release is out, you'll download a small **installer**. You point it at **your
own disc image**; it checks that the disc is the right one and **builds the game on your
computer from your disc**. Nothing from the game is ever downloaded from here.

- Windows: the installer produces the game executable.
- Android: the same installer, run on your PC, builds the app and installs it on your phone.

**There is no release yet.** Watch the repository or join the Discord to know when testing opens.

## Touch controls (Android)

![Touch control icons](images/touch_icons.png)

The buttons follow what the game is doing, whether you're exploring, aiming, in a QTE, in a menu,
in the attaché case or in a cutscene:
- floating stick, and a run button you can lock with a double tap;
- aim, fire, knife, reload, attaché case, map and quick turn;
- QTEs become **tap / swipe / mash** prompts, including the boulder chase;
- tap menu text directly; tap an item in the attaché case, then *Equip*;
- a skip button that only appears in cutscenes.

## Reporting bugs

Open an issue with your platform, device or GPU, what you were doing, and a screenshot if you
can. **Never attach game files or disc images.**

## Legal

- This project is **not affiliated with or endorsed by Capcom or Nintendo**. *Resident Evil* is a
  trademark of Capcom.
- **No game data, ISOs or game code are distributed**, here or anywhere else. Requests for
  them will be closed.
- Non-commercial: no paid versions, no subscriptions, no ads.

## Credits

- **Volodymyr Vovchok / BlackLine Interactive**: [NWiiRecomp](https://github.com/BlackLineInteractive/NWiiRecomp),
  the recompiler this port is built on. Used under its license: non-commercial use, with
  attribution.
- The **RE4 GameCube decompilation** project.
- **SDL2**, for windowing, input and audio.
- **Dolphin** and **YAGCD**, for GameCube hardware documentation.
- **Dusklight**, **melee-pc**, **UnleashedRecomp-Android**, **N64Recomp / PS2Recomp**: for
  inspiration and their public notes.

## Community

Discord: https://discord.gg/AXAzExECSv

*— ayano*
