# Oracle PS2 : nos conversions de boutisme contre celles de Capcom

Le port PS2 de Capcom (disque « Review Debug », BIO4DAT.AFS) contient les memes formats que la GameCube, deja
convertis en petit-boutiste par Capcom. On convertit les fichiers GameCube avec NOTRE code (port/endian.cpp) sur PC
et on compare, mot par mot, a la version de Capcom.

- `afs_iso.py <iso> list|get <noms...> <dossier>` : lit BIO4DAT.AFS directement dans l'ISO.
- `harness.cpp` : applique port_endian_fix (ou port_room_arc_fix avec le nom `room`) a un fichier ; compile avec
  g++ 64 bits (`-DRE4_PORT -Iinclude -Iport`, avec port/endian.cpp).
- `unyz2.cpp` : decompresse une salle (YZ2) avec le decodeur du jeu (src/game/yz2code.cpp + port/yz2.cpp) ; memoire
  de travail prise sous 4 Go (le decodeur range des adresses en u32).
- `cmp3.py <gc> <notre> <ps2> [entree]` : 'O' = mot que Capcom a retourne et pas nous ; 'E' = l'inverse.
- `sweep.py <salles_gc> <iso>` : toutes les salles ; etiquettes non converties + oublis par etiquette.

A savoir avant de conclure :
- Capcom a laisse certaines tables gros-boutistes sur PS2 et les relit retournees (core.das entree 2).
- Notre jeu lit expres certaines donnees gros-boutistes (valeurs des FCV : FccF32/FccS16) : oublis attendus.
- Plusieurs structures PS2 ont une autre disposition (ESE, ETS, SEQ des personnages, UWF, MDT) : comparer au code.
- Toujours un test de non-regression : convertir tous les fichiers avec l'ancienne et la nouvelle version.
