/* Target configuration for building gcc 2.95.3's libgcc2.c for the GameCube (PowerPC EABI, big
 * endian, 32-bit words, sjlj exceptions) without the compiler's own headers. No INIT_SECTION:
 * __main/__do_global_ctors run the constructor list. */
#ifndef LIBGCC2_TCONFIG_H
#define LIBGCC2_TCONFIG_H

#define inhibit_libc

#define BITS_PER_UNIT 8
#define UNITS_PER_WORD 4
#define MIN_UNITS_PER_WORD 4
#define WORDS_BIG_ENDIAN 1
#define BYTES_BIG_ENDIAN 1
#define LONG_DOUBLE_TYPE_SIZE 64

#define SUPPORTS_WEAK 0
#define PROTO(x) x

extern void abort(void) __attribute__((noreturn));
extern void *malloc(unsigned int);
extern void free(void *);
extern void *memset(void *, int, unsigned int);
extern int strcmp(const char *, const char *);
extern int write(int, const void *, unsigned int);

#endif
