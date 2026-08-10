# Le vrai mur du portage : le GameCube est gros-boutiste

## Ce qui a ete mesure

Le portage demarre, lit le disque de l'utilisateur, initialise la memoire, la video, GX, l'ARAM
et les threads, et arrive dans `SndInit()`. La il tombe, et la cause n'est pas une fonction
manquante.

`SndInit` lit `bgm/bio4str.hed` puis fait, tel quel :

```c
SndMem.str_file[i] = (SndStrFile*) (SND_DATA_TOP + ((u32*) SND_DATA_TOP)[i]);
```

Les seize premiers octets de ce fichier, sur le disque reel :

```
00 00 00 20  00 00 25 80  00 00 00 00  00 00 00 00
```

* lus en **gros-boutiste** (le GameCube) : `0x20`, `0x2580` - deux decalages plausibles ;
* lus en **petit-boutiste** (ARM) : `0x20000000`, `0x80250000` - deux pointeurs sauvages.

Le jeu part alors ecrire n'importe ou. Ce n'est pas un bogue du portage : c'est que **toutes les
donnees de RE4 sont rangees en gros-boutiste**, et que le code decompile les lit avec des acces
ordinaires, parce que son processeur l'etait aussi.

## Pourquoi il n'y a pas de correctif local

Trois voies ont ete examinees :

1. **Compiler le jeu en gros-boutiste.** `armeb` existe dans l'architecture, mais aucune
   plateforme visee ne le propose : le NDK Android ne fournit pas de sysroot gros-boutiste, et
   AArch64 - donc iOS - n'a plus du tout ce mode. Voie fermee.
2. **Basculer l'endianness des donnees a l'execution** (`SETEND BE` / bit E du CPSR sur ARMv7).
   Techniquement reel, mais il faudrait rebasculer autour de chaque appel systeme et de chaque
   fonction de la plateforme, l'instruction est depreciee en ARMv8 et absente en AArch64, et
   certains noyaux la desactivent. Fragile, et sans avenir sur iOS.
3. **Retourner les octets a la lecture.** C'est la seule voie qui tient, et c'est celle que
   prennent les portages de decompilations qui aboutissent.

## La forme que cela doit prendre

Retourner les octets ne peut pas se faire "en gros" : un fichier de RE4 melange des entiers
32 bits, des 16 bits, des flottants et des octets. Retourner tout par mots de quatre casserait
les chaines et les tableaux d'octets ; ne rien retourner casse tout le reste. **Il faut un
convertisseur par format**, qui connaisse la structure.

La bonne architecture, et elle tombe bien avec le modele de distribution :

> Au premier lancement, l'application demande son disque a l'utilisateur **et le convertit une
> fois** en un cache a l'endianness de la machine. Les lancements suivants lisent le cache.

C'est une etape que l'utilisateur comprend (les emulateurs et les portages font pareil), elle ne
se paie qu'une fois, et elle laisse le code du jeu intact - aucun acces a reecrire dans les
520 000 lignes.

## Ce qui rend le travail faisable ici

Le projet possede deja les lecteurs de ces formats, en Python, ecrits et verifies au fil des
annees : `re4tpl.py` pour les textures, les outils `.das` / `.drs`, les tables d'items, les
salles. Un lecteur qui sait interpreter un format sait aussi dire ou sont ses entiers - c'est
la meme connaissance, exprimee autrement.

Et le decompile donne la seconde moitie : **les declarations C++ de chaque structure lue depuis
le disque**. De quoi engendrer les convertisseurs plutot que les ecrire a la main.

## Ordre de travail propose

Le chemin de demarrage ne touche qu'une poignee de formats. Les convertir dans l'ordre ou le jeu
les demande - la meme methode que pour les fonctions de plateforme, et pour la meme raison :
c'est l'execution qui dit ce qui compte.

1. `bgm/bio4str.hed` et les tables de son (ce qui bloque maintenant) ;
2. `etc/sizetbl.dat`, `etc/core.das` (tables d'objets et d'interface) ;
3. l'archive de salle et ses textures TPL ;
4. les modeles et leurs listes d'affichage.
