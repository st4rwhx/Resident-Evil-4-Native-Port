# Couche de portage ARM (Android d'abord, iOS ensuite)

Le decompile reproduit `main.dol` et 110 RELs a l'octet pres. Ce dossier ne touche pas a ce
build : il ajoute une **couche de plateforme** qui remplace le SDK Nintendo, activee par
`-DRE4_PORT`. Sans cette macro, rien ici n'existe et le build matching est inchange.

## Etat mesure

| | |
|---|---|
| Unites de jeu qui compilent en `armeabi-v7a` | **536 / 618 (86 %)** |
| Objets ARM produits | 515 |
| Fonctions SDK a fournir | **249** (mesurees : 4903 sites d'appel) |
| **Bibliotheque liee** | **`libre4dol.so`, ELF32/ARM, 1,3 Mo de `.text`, 5393 symboles** |
| Symboles non definis / doublons au lien | **0 / 0** |
| **Fonctions de plateforme reelles et verifiees sur l'appareil** | **83 / 249** |

Elle **lie**, elle ne **tourne pas** : les 249 fonctions sont des souches qui journalisent leur
premier appel et rendent une valeur neutre. C'est deliberé - a partir d'un binaire qui lie, on
remplace les fonctions une par une en lisant a chaque execution ce que le jeu reclame ensuite,
au lieu de tout ecrire a l'aveugle puis d'esperer.

## Pourquoi 32 bits

`types.h` fait `typedef unsigned long u32`, juste sur l'ABI 32 bits du GameCube et faux en LP64,
ou `unsigned long` fait 8 octets. Au-dela du typedef, le jeu range des pointeurs dans des
structures dont la disposition est fixee par les formats de fichiers. En `armeabi-v7a` le
probleme n'existe pas. **iOS est 64 bits uniquement**, et demandera donc l'audit complet des
hypotheses de taille de pointeur : Android d'abord, iOS ensuite et plus cher.

## Ce qui a debloque la compilation

L'essentiel vient des options, a cout nul : `-std=gnu++11` supprime les 526 erreurs de struct
anonyme a constructeur non trivial (unions non restreintes), `-fdeclspec` en supprime 369, et
compiler les `.c` en C supprime les 178 `new` comme nom de parametre. Pas `-std=gnu++17`, qui
rouvre 44 erreurs en supprimant `register`.

Cote source, quatre points seulement, tous sous `#ifdef RE4_PORT` :

- `math_sub.h` : `fabsf` est ecrit en asm PowerPC. **Une ligne, 351 unites sur 969 en echec**,
  parce que l'en-tete arrive partout via `player.h`.
- `cManager.h` : `arrayPush` appelle `Debug_alloc` sans inclure `main_mem.h`.
- `card.h` : `new` comme nom de parametre (le SDK est du C).
- `gx/GXGeometry.h` : redeclare `OSPanic` sans `const`, en conflit avec `os.h`.

## Deux choses apprises au lien

**Les RELs ne peuvent pas etre lies a plat.** `_prolog` existe dans `ss_main.cpp` et dans
`em11_set.cpp` : chaque overlay a ses symboles locaux, et le portage doit garder la separation
DOL / RELs. La bibliotheque ci-dessus ne contient donc que les unites du DOL.

**La soumission de sommets est deja une frontiere de liaison.** `GXVert.h` genere
`GXPosition3f32` & co. par macro ; sous `#if DEBUG` - que le build utilise deja - ce sont des
**fonctions externes** et non des ecritures inline dans le write-gather pipe. Les 228 appels a
`GXPosition3f32`, 187 a `GXTexCoord2f32` et 168 a `GXColor4u8` se redirigent donc vers un tampon
hote sans toucher une ligne du jeu.

## Textures

Recensement du corpus reel avant d'ecrire une ligne : **227 TPL du jeu, 1002 images, trois
formats seulement** - CMPR (534), IA8 (367), I4 (101). Les huit autres formats que GX definit
n'apparaissent nulle part dans RE4 GameCube. Ils sont quand meme implementes, mais on sait
desormais ou porte le risque.

Le decodeur est la transcription exacte de `tools/re4tpl.py` (depot RE4444), et il est **juge
contre lui** : `port/make_tex_cases.py` fabrique des cas depuis de vraies textures du jeu (plus
du bruit reproductible pour les formats absents), et `port/test_texture.cpp` exige l'egalite
**octet a octet** sur l'appareil. **33 cas sur 33, 11 formats, zero pixel d'ecart.**

Cette exigence n'est pas du zele : une tuile mal dimensionnee (I8 et IA4 font 8x4, pas 8x8)
produit une image parfaitement plausible et fausse, et RGBA8 lu en quadruplets reste lisible.
Aucun de ces deux bugs ne se voit a l'oeil ; tous deux sautent au premier octet compare.

## Suite

1. **TEV -> shaders generes.** Le shader actuel a un seul etage (couleur de sommet x texture) ;
   TEV en a jusqu'a seize. Dolphin est la reference.
2. Eclairage et generation de coordonnees de texture (`GXSetTexCoordGen`, matrices de texture).
3. AX -> audio, CARD -> bac a sable.
4. Coque Android et interface tactile, puis l'audit 64 bits pour iOS.

## Fichiers

- `gen_stubs.py` — genere `stubs_generated.cpp` depuis les prototypes de `include/dolphin/`,
  donc les signatures sont exactes par construction et non retapees.
- `stubs_generated.cpp` — GENERE, ne pas editer.
- `port_log.h` / `port_log.cpp` — journal du premier appel, sans STL (les macros du decompile
  cassent `<set>` de libc++).
- `texture.cpp` — decodage des formats GX vers RGBA8. Aucune dependance GL : testable seul.
- `texobj.cpp` — `GXTexObj` / `GXTlutObj` poses sur des textures GLES, avec cache et
  invalidation. `PortTexObj` tient dans les 32 octets de `GXTexObj`, `static_assert` a l'appui.
- `build_tests.sh` — construit, pousse et prepare les harnais. Il porte les trois pieges de
  l'environnement (MSYS_NO_PATHCONV, le rechmod apres `adb push`, l'espace dans "RE4 PROJECT"
  qui impose un fichier reponse avec un chemin entre guillemets par ligne).
- `make_tex_cases.py` — fabrique les cas de test des textures depuis de vrais TPL.

## Une correction generale

`AT_ADDRESS` s'evaporait sous GCC, laissant une trentaine de variables materielles
**definies** dans des en-tetes : en C ce sont des symboles communs que l'editeur de liens
fusionne, en C++ ce sont des definitions, et deux unites suffisent a casser le lien. Sous
`RE4_PORT`, `AT_ADDRESS` vaut maintenant `__attribute__((weak))`, ce qui retablit la semantique
du C d'origine en un point unique - au lieu de `--allow-multiple-definition`, qui aurait aussi
masque les vrais doublons.
