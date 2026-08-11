# Aurora : la couche GameCube qui existait deja

## Ce que c'est, et pourquoi on s'en sert

[`encounter/aurora`](https://github.com/encounter/aurora), sous licence MIT, se decrit comme
« a source-level GameCube & Wii compatibility layer **intended for use with game decompilation
projects** ». C'est exactement notre situation, et c'est, ligne pour ligne, la couche que le
portage ecrivait a la main.

Mesure contre notre code :

| | |
|---|---|
| en-tetes du SDK Dolphin | **93 des 99** que le decompile inclut |
| fonctions GX que RE4 appelle | **207 sur 303** couvertes |
| ce qui manque | compteurs de performance, macros de registres, une poignee de getters |
| generateur TEV -> shader | **2 056 lignes**, deja ecrites |

Le precedent existe aussi : [melee-pc](https://github.com/turtlesoupy/opensmash-melee-pc)
construit le decompile de Super Smash Bros. Melee sur aurora, avec le meme modele de
distribution que le notre - *« No game data; bring your own disc »*.

## Ce qu'elle ne couvre pas

L'audio (`ax`, `axfx`, `mix`, `seq`, `syn`, `sp`), les en-tetes de demonstration, `G2D`, et
`vi/vifuncs.h`. Le portage garde donc sa propre couche audio - inerte pour l'instant - et sa
lecture d'image disque, qui connait les particularites de RE4 (lecture par secteurs bruts, et
une taille de fichier qui ne borne pas la lecture).

## Le point dur : elle engendre du WGSL

Le generateur de shaders d'aurora emet du **WGSL**, avec des tampons de stockage
(`var<storage, read>`) pour aller chercher les sommets. C'est du WebGPU natif : il faut donc
Dawn, et Tint, pour descendre jusqu'a GLSL ES. Ce n'est pas un obstacle sur PC ; ce sera la
question a trancher pour le Galaxy S5 Plus, dont le GPU n'a pas de pilote Vulkan et dont
l'architecture 32 bits n'est pas couverte par les paquets Dawn preconstruits (windows-amd64,
linux-x86_64, darwin, ios-arm64, **android-aarch64** seulement).

## Construire aurora avec GCC sous Windows

Il faut GCC, et pas clang : voir `ENDIANNESS.md`, c'est `scalar_storage_order` qui l'impose.
Or aurora vise MSVC ou clang sous Windows. Six obstacles, tous franchis ; ils sont notes ici
parce qu'aucun n'est devinable.

1. **Chemin sans espace.** Le compilateur de ressources coupe sur l'espace de
   « RE4 PROJECT ». Tout se construit sous `C:\re4port`. (Le meme piege avait deja frappe
   clang et les fichiers de reponse.)

2. **Paquet de certificats.** Le cmake de MinGW n'en embarque aucun, et tous les
   telechargements echouent sur « certificate signer not trusted ». Donner celui de Git :

   ```
   -DCMAKE_TLS_CAINFO="C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt"
   ```

3. **La Dawn preconstruite est inutilisable.** Elle est compilee avec Visual Studio, et
   aurora appelle son **API C++** (`dawn::native::DawnInstanceDescriptor`), pas seulement le
   `webgpu.h` en C : les noms decores ne correspondent pas. Il faut donc
   `-DAURORA_DAWN_PROVIDER=vendor`, c'est-a-dire construire Dawn depuis ses sources.

4. **`O_NONBLOCK` / `O_CLOEXEC`** n'existent pas sous MinGW, et l'abseil embarque de Dawn les
   utilise. `-DO_NONBLOCK=0 -DO_CLOEXEC=0` suffit : le chemin concerne ne sert pas sous
   Windows.

5. **Dawn compile sa couche D3D meme quand les backends D3D sont eteints**, et reclame
   `DXProgrammableCapture.h`, un en-tete du SDK Windows absent de MinGW. L'interrupteur est
   `-DDAWN_USE_WINDOWS_UI=OFF` : il ne gouverne que les chaines d'echange D3D, inutiles en
   Vulkan.

6. **`comsuppw`**, une bibliotheque de Visual Studio, est reclamee par l'interface de Tracy
   alors que le tracage est desactive. Rien n'en est appele : une archive vide la satisfait
   (`ar rcs libcomsuppw.a` sur un objet vide).

Et un dernier, du cote du point d'entree : `SDL_main.h` renomme `main` et attend la
bibliotheque `SDL3_main`, elle aussi au format Visual Studio. On definit `SDL_MAIN_HANDLED`
et on appelle `SDL_SetMainReady()` - un correctif local de trois lignes dans `lib/main.cpp`.

### La commande

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DAURORA_ENABLE_DVD=OFF -DAURORA_ENABLE_CARD=OFF \
  -DAURORA_DAWN_PROVIDER=vendor -DAURORA_DAWN_LINKAGE=static \
  -DDAWN_ENABLE_D3D12=OFF -DDAWN_ENABLE_D3D11=OFF -DDAWN_ENABLE_VULKAN=ON \
  -DDAWN_USE_WINDOWS_UI=OFF \
  -DCMAKE_C_FLAGS="-DO_NONBLOCK=0 -DO_CLOEXEC=0" \
  -DCMAKE_CXX_FLAGS="-DO_NONBLOCK=0 -DO_CLOEXEC=0" \
  -DCMAKE_EXE_LINKER_FLAGS="-LC:/re4port/stublib -mconsole" \
  -DCMAKE_TLS_CAINFO="C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt"
cmake --build build --target simple
```

`AURORA_ENABLE_DVD=OFF` parce que le paquet `nod` preconstruit n'a pas de bibliotheque pour
MinGW - et que notre lecteur d'image disque fonctionne deja.

### Verifie

L'exemple `simple` demarre, ouvre sa fenetre, obtient un adaptateur Vulkan et initialise le
rendu :

```
[INFO: aurora::gpu;Attempting to initialize Vulkan]
[INFO: aurora::gpu;Graphics adapter information
  API: Vulkan
  Device: NVIDIA GeForce GTX 1660 Ti (DiscreteGPU)]
[INFO: aurora;Using framebuffer size 1280x960 scale 1]
```

Tout le chemin tient donc : GCC -> Dawn depuis les sources -> aurora -> Vulkan.

## Les correctifs locaux apportes a aurora

Ils sont peu nombreux et tous du meme genre : revenir au comportement du SDK Nintendo, ou
tenir compte du fait que le jeu vient d une machine 32 bits. Aucun ne change ce qu aurora
fait, seulement ce qu elle refuse.

| ou | quoi | pourquoi |
|---|---|---|
| `lib/dolphin/os/OSMemory.cpp` | MEM1 demande a `0x80000000`, sinon sous 4 Go | RE4 range des pointeurs dans des `u32`. Au-dessus de 4 Go, chaque aller-retour perd les bits hauts. |
| `lib/card/DolphinCardPath.cpp` | `shlobj.h` au lieu de `shlobj_core.h` | MinGW ne livre pas le second, un decoupage recent du SDK Windows. |
| `lib/dolphin/card.cpp` | callback nulle acceptee (10 endroits) | Le SDK la traite comme une requete synchrone. RE4 s en sert. |
| `lib/dolphin/card.cpp` | `CARDProbeEx` accepte des pointeurs de sortie nuls | Idem le SDK ; RE4 le fait dans `createSysfile`. |
| `lib/gx/texture.cpp` | refuse une texture de taille invraisemblable au lieu d avorter | Le jeu peut lier un `GXTexObj` jamais initialise. Sur GameCube cela dessinait du bruit ; `abort()` est bien plus severe que le materiel. |
| `lib/gx/fifo.cpp` | `ProcessingMode::Drain` au lieu de `Thread` | Le fil d analyse pouvait tourner entre deux images, hors enregistrement. A revoir quand la cadence le demandera. |
| `lib/main.cpp` | `SDL_MAIN_HANDLED` + `SDL_SetMainReady()` | `SDL_main.h` renomme `main` et reclame une bibliotheque au format Visual Studio. |

## Ou se branche le portage

Trois points, et le troisieme a demande deux essais :

1. **`port/main_pc.cpp`** initialise aurora avant `re4_main`.
2. **`port/wgpipe_pc.cpp`** relie le write-gather pipe du Gekko a `aurora::gx::fifo::write_*`.
   Sans lui, `GXBegin` annonce n sommets dont les octets ne suivent jamais et tout le flux de
   commandes glisse - le symptome etait un « unsupported primitive type 240 ».
3. **`port/vi_pc.cpp`** pompe les images. Le piege : la boucle principale de RE4 **n appelle
   pas `VIWaitForRetrace`**. Elle termine son image par `Render_swap`, soit
   `VISetNextFrameBuffer` puis `VIFlush`, et se synchronise sur son propre compteur de vsync.
   `VIWaitForRetrace` ne sert qu au demarrage. Tant que la pompe etait la, tout le jeu
   dessinait dans une seule image d aurora, dont les 24 Mo de tampon d uniformes finissaient
   par deborder.
