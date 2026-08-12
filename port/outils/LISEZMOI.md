# Outils du portage natif (phase N1, 11/11/2026)

- `laptop_port_build.sh [PORT_OPT]` : copie le depot sur le portable (ssh admin@192.168.1.124, C:/re4n/re4port), corrige la
  collision de casse `src/Tools` / `src/tools` dans la copie (`casse_tar.py` : contenus pris dans git, minuscules dans
  `src/tools_lc`, `modules.py` et les `#include "tools/` rediriges), puis compile dans l'ordre : objets du DOL, modules REL
  (`build_rel.py` lit `_port_build/obj` pour savoir ce que le DOL definit), lien. -O2 par defaut.
- `port_cycle.sh <secondes> [RE4_*=...]` : compile, pousse `re4_n1` sur le S5 (/data/local/tmp/re4), lance avec le disque extrait
  (`/sdcard/Android/data/org.nightjar.natif/files/game/disc1`), resume le journal.
- `pad8.txt` : parcours de manette qui se rattrape (B, Start, Haut, A toutes les 400 lectures) : nouvelle partie puis village
  avec `RE4_START_ROOM=0x100`. Le menu titre demarre sur LOAD (curseur 1), Nouvelle partie est au-dessus.
