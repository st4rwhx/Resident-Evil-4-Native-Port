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

## Suite

1. Implementer dans l'ordre que dicte `[PORT] non implemente :` a l'execution : OS/memoire,
   puis DVD (lire l'ISO de l'utilisateur), puis VI.
2. GX -> OpenGL ES 3.0 : ~110 fonctions, le gros morceau, mais TEV se traduit en shaders generes.
3. PAD -> tactile, AX -> audio, CARD -> bac a sable.
4. Coque Android, puis l'audit 64 bits pour iOS.

## Fichiers

- `gen_stubs.py` — genere `stubs_generated.cpp` depuis les prototypes de `include/dolphin/`,
  donc les signatures sont exactes par construction et non retapees.
- `stubs_generated.cpp` — GENERE, ne pas editer.
- `port_log.h` / `port_log.cpp` — journal du premier appel, sans STL (les macros du decompile
  cassent `<set>` de libc++).
