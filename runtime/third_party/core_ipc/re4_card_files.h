/* Files on the port's memory card for the app's menu (M11): list, export and
 * import as .gci, the file format of Dolphin and of the GameCube's copy tools
 * (a 64-byte directory entry, then the file's blocks, all in GameCube byte
 * order). runtime/card_files.cpp works on the card image of
 * runtime/memcard.cpp in the way of the SDK's card library (two copies of
 * directory and FAT, the newer one counts); runtime/gci.cpp turns the game's
 * own files (bh4_system, bh4_dataNN) between the port's byte order and the
 * GameCube's, field by field (runtime/save_layout.inc).
 *
 * Only while the game stands and the card library has the card unmounted
 * (core_api.cpp checks both); the functions themselves take no locks. */
#ifndef RE4_CARD_FILES_H
#define RE4_CARD_FILES_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RE4_CARD_OK = 0,
    RE4_CARD_NOCARD = -1,   /* slot A empty or the card not formatted */
    RE4_CARD_EXISTS = -2,   /* a file of that name is there (import without replace) */
    RE4_CARD_NOSPACE = -3,  /* not enough free blocks or directory entries */
    RE4_CARD_BAD = -4,      /* not a .gci of this game, or broken */
    RE4_CARD_BUSY = -5,     /* the game uses the card (core_api.cpp) */
    RE4_CARD_NOFILE = -6,
};

#define RE4_CARD_BLOCK 0x2000u
#define RE4_GCI_HEADER 0x40u

typedef struct Re4CardFile {
    int index;           /* directory entry 0..126 */
    char game[5];        /* "G4BE" */
    char company[3];     /* "08" */
    char name[33];       /* "bh4_data00" */
    unsigned blocks;
    unsigned time;       /* seconds since 2000-01-01 (OSTicksToSeconds) */
    char comment[2][33]; /* the two comment lines, "" when none */
    /* the game's save header (bh4_dataNN, 0 otherwise): room (stage << 8 |
       room), chapter, times saved, play time in seconds, saved
       (year, month 1-12, day, hour, minute), the CRCs held */
    int save;
    unsigned room, chapter, saveCount, playTime;
    int year, month, day, hour, minute;
    int intact;
} Re4CardFile;

/* The files on the card (at most `max`); RE4_CARD_NOCARD when there is no
   formatted card. */
int re4_card_list(Re4CardFile* out, int max);
/* Free blocks of the card, -1 without one. */
int re4_card_free_blocks(void);
/* The directory entry (64 bytes, port order) and the blocks of entry `index`;
   `data` holds `capacity` bytes (0: the entry only). The file's bytes, or an
   RE4_CARD_* error. */
int re4_card_read(int index, unsigned char* dir, unsigned char* data, unsigned capacity);
/* Writes a file: `dir` its directory entry (port order; startBlock is
   chosen here), `data` its length * 8 KB. replace: a file of the same game,
   company and name goes first. RE4_CARD_OK or an error. */
int re4_card_write(const unsigned char* dir, const unsigned char* data, int replace);

/* The .gci of entry `index` into `out` (capacity bytes; 0x40 + 8 KB per
   block): its size, or an error; `note` says what was done or why not. */
int re4_gci_export(int index, unsigned char* out, unsigned capacity, char* note, int noteSize);
/* A .gci onto the card (replace: over a file of the same name). */
int re4_gci_import(const unsigned char* gci, unsigned size, int replace, char* note, int noteSize);
/* The name of entry `index` as Dolphin names a .gci in its GCI folder
   ("08-G4BE-bh4_data00.gci"); 0 or an error. */
int re4_gci_file_name(int index, char* out, int size);
/* The contents of one of the game's files (`name`, `bytes` long) from the
   port's byte order to the GameCube's (toGameCube = 1) or back, CRCs anew.
   RE4_CARD_OK, RE4_CARD_BAD when it is not one of the game's files or its
   CRCs do not hold (note: which). */
int re4_gci_convert(const char* name, unsigned char* data, unsigned bytes, int toGameCube, char* note, int noteSize);

#ifdef __cplusplus
}
#endif

#endif
